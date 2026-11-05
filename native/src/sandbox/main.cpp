// Sandbox, Phase 3: a scene made of entities and components.
//
// Everything on screen is an entity in a Scene: the camera, the sun, the
// ground, the crates, the ball and the little solar system. The sandbox adds
// two components of its own, Spin and Patrol, to show that any plain struct
// can be a component, and that a "system" is just a loop over the entities
// that have the components it needs.
//
// What to look at:
//   * The orrery (right): the planet is a child of the spinning arm, and the
//     moon a child of the planet. Nothing moves the moon directly; it is
//     carried by its parents. Press B to detach the planet: it stays where it
//     is (keepWorldTransform) and stops orbiting, but the moon still circles
//     it. B again re-attaches it.
//   * The ball patrols through the row of crates. Each fixed step asks the
//     collision code what the ball overlaps, and those crates turn red.
//     C shows the colliders as green wireframes.
//   * N spawns a crate in front of the camera, Backspace removes the newest,
//     Delete removes the whole "Spawned" group: destroying a parent destroys
//     its children.
//   * Tab prints the scene's hierarchy to the log (a preview of Phase 5's outliner).
//   * P pauses the simulation. Everything still renders: rendering and
//     simulation are separate.
//   * Hold the right mouse button to fly (WASD, Q/E, Shift, scroll). J/L/I/K
//     move the sun. R reload shaders, F1 wireframe, V vsync, F12 screenshot,
//     H help, Esc quits.

#include "core/Application.h"
#include "core/Log.h"
#include "core/Math.h"
#include "core/Paths.h"
#include "core/Screenshot.h"
#include "render/Camera.h"
#include "render/FlyCameraController.h"
#include "render/ForwardRenderer.h"
#include "render/Image.h"
#include "render/Mesh.h"
#include "render/Primitives.h"
#include "render/Texture.h"
#include "scene/Collision.h"
#include "scene/Scene.h"
#include "scene/SceneRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace Engine;

namespace {

// ---------------------------------------------------------------- the sandbox's own components
// Turns the entity around an axis at a steady rate.
struct Spin {
    Vec3 axis{0.0f, 1.0f, 0.0f};
    float degreesPerSecond = 45.0f;
};

// Moves the entity back and forth along X between two points.
struct Patrol {
    float minX = -5.0f;
    float maxX = 5.0f;
    float speed = 2.0f; // m/s; the sign is the current direction
};

// Turns red while something overlaps it. Remembers the normal color.
struct HitHighlight {
    Vec3 normalColor{1.0f};
};

struct Options {
    long long frames = 0;
    bool vsync = true;
    std::string screenshotPath;
};

class Sandbox final : public Application {
public:
    Sandbox(ApplicationSettings settings, std::string screenshotPath)
        : Application(std::move(settings)), screenshotPath_(std::move(screenshotPath)) {}

protected:
    void onStart() override {
        renderer_ = std::make_unique<ForwardRenderer>();
        sceneRenderer_ = std::make_unique<SceneRenderer>(*renderer_);
        const IVec2 fb = window().framebufferSize();
        renderer_->setViewport(fb.x, fb.y);

        loadResources();
        buildScene();
        freeCamera_.position = Vec3(1.0f, 3.0f, 7.0f);
        freeCamera_.lookAt(Vec3(1.8f, 0.9f, -2.5f));
        followFreeCamera();
        printHelp();
        printHierarchy();
    }

    void onFixedUpdate(double dtSeconds) override {
        // First, always: remember where everything is, so rendering can blend
        // from here to wherever this step moves things.
        scene_.beginFixedStep();
        if (paused_) {
            return;
        }
        const float dt = static_cast<float>(dtSeconds);
        spinSystem(dt);
        patrolSystem(dt);
        hitHighlightSystem();
    }

    void onUpdate(double frameSeconds) override {
        const float dt = static_cast<float>(frameSeconds);
        flyCamera_.update(freeCamera_, input(), dt);
        if (flyCamera_.wantsCursorCaptured() != window().cursorCaptured()) {
            window().setCursorCaptured(flyCamera_.wantsCursorCaptured());
        }
        followFreeCamera();
        moveSun(dt);
        handleKeys();
        renderer_->reloadShadersIfChanged(frameSeconds);
        refreshTitle(frameSeconds);
    }

    void onRender(double alpha) override {
        const IVec2 fb = window().framebufferSize();
        if (fb.x == 0 || fb.y == 0) {
            return; // minimized
        }
        sceneRenderer_->render(scene_, static_cast<float>(fb.x) / static_cast<float>(fb.y), static_cast<float>(alpha));
        maybeTakeScreenshot(fb);
    }

    void onFramebufferResize(int width, int height) override {
        if (renderer_) {
            renderer_->setViewport(width, height);
        }
    }

private:
    // ---------------------------------------------------------------- systems
    // A system is a function that loops over the entities with certain
    // components. It holds no data of its own; all state is in components.
    void spinSystem(float dt) {
        scene_.registry().each<Spin, Transform>([&](Entity, Spin& spin, Transform& t) {
            const Quat step = glm::angleAxis(Math::toRadians(spin.degreesPerSecond) * dt, glm::normalize(spin.axis));
            t.rotation = glm::normalize(step * t.rotation);
        });
    }

    void patrolSystem(float dt) {
        scene_.registry().each<Patrol, Transform>([&](Entity, Patrol& p, Transform& t) {
            t.position.x += p.speed * dt;
            if (t.position.x > p.maxX || t.position.x < p.minX) {
                t.position.x = Math::clamp(t.position.x, p.minX, p.maxX);
                p.speed = -p.speed; // turn around
            }
        });
    }

    void hitHighlightSystem() {
        // Collision detection runs in the fixed step, on the current (not
        // blended) positions, so the result is the same at any frame rate.
        const std::vector<Entity> hits = overlapping(scene_, ball_);
        overlapCount_ = static_cast<int>(hits.size());
        scene_.registry().each<HitHighlight, MeshRenderer>([&](Entity e, HitHighlight& h, MeshRenderer& mr) {
            const bool hit = std::find(hits.begin(), hits.end(), e) != hits.end();
            mr.material.baseColor = hit ? Vec3(1.0f, 0.25f, 0.2f) : h.normalColor;
        });
    }

    // ---------------------------------------------------------------- setup
    void loadResources() {
        cube_ = std::make_unique<Mesh>(Primitives::cube());
        plane_ = std::make_unique<Mesh>(Primitives::plane());
        sphere_ = std::make_unique<Mesh>(Primitives::sphere(0.5f, 48, 24));
        std::string error;
        if (auto texture = Texture::fromFile(assetPath("textures/crate.png"), {}, &error)) {
            crate_ = std::make_unique<Texture>(std::move(*texture));
        } else {
            ENGINE_WARN("Using a checkerboard instead of the crate texture: ", error);
            crate_ = std::make_unique<Texture>(makeCheckerboard(64, 4, Vec3(0.8f, 0.5f, 0.2f), Vec3(0.4f, 0.2f, 0.1f)));
        }
        ground_ = std::make_unique<Texture>(makeCheckerboard(256, 8, Vec3(0.55f), Vec3(0.42f)));
    }

    // Small helpers so the scene description below reads like a list.
    Entity addMesh(const std::string& name, const Mesh& mesh, const Material& material, const Vec3& position,
                   const Vec3& scale = Vec3(1.0f), Entity parent = NullEntity) {
        const Entity e = scene_.createEntity(name);
        if (!parent.isNull()) {
            scene_.setParent(e, parent, false); // numbers below are relative to the parent
        }
        Transform& t = scene_.get<Transform>(e);
        t.position = position;
        t.scale = scale;
        MeshRenderer& mr = scene_.add<MeshRenderer>(e);
        mr.mesh = &mesh;
        mr.material = material;
        return e;
    }

    Material crateMaterial() const {
        Material m;
        m.albedo = crate_.get();
        m.specular = 0.1f;
        m.shininess = 16.0f;
        return m;
    }

    Material colored(const Vec3& color, float specular = 0.4f, float shininess = 32.0f) const {
        Material m;
        m.baseColor = color;
        m.specular = specular;
        m.shininess = shininess;
        return m;
    }

    void buildScene() {
        // Camera and sun.
        cameraEntity_ = scene_.createEntity("Main Camera");
        scene_.add<CameraComponent>(cameraEntity_);
        sunEntity_ = scene_.createEntity("Sun");
        scene_.add<LightComponent>(sunEntity_);

        Material ground;
        ground.albedo = ground_.get();
        ground.uvScale = Vec2(25.0f); // 200 m plane, 1 m squares
        ground.specular = 0.05f;
        addMesh("Ground", *plane_, ground, Vec3(0.0f), Vec3(200.0f, 1.0f, 200.0f));

        // A row of crates under one empty parent entity, used like a folder.
        const Entity row = scene_.createEntity("Crate Row");
        for (int i = 0; i < 3; ++i) {
            const Entity crate = addMesh("Crate " + std::to_string(i + 1), *cube_, crateMaterial(),
                                         Vec3(-3.0f + 3.0f * static_cast<float>(i), 0.5f, -1.0f), Vec3(1.0f), row);
            scene_.add<Collider>(crate); // a 1 m box, the cube's size
            scene_.add<HitHighlight>(crate);
        }

        // The ball: a sphere collider that patrols through the crates.
        ball_ = addMesh("Ball", *sphere_, colored(Vec3(0.25f, 0.5f, 1.0f), 0.6f, 64.0f), Vec3(-5.0f, 0.5f, -0.4f),
                        Vec3(0.7f)); // z = -0.4: grazes the crates' front faces, so it stays visible
        Collider& ballCollider = scene_.add<Collider>(ball_);
        ballCollider.shape = ColliderShape::Sphere; // radius 0.5 x scale 0.7 = 0.35 m, the mesh's size
        scene_.add<Patrol>(ball_, -5.0f, 5.0f, 2.0f);

        buildOrrery();
        spawnGroup_ = scene_.createEntity("Spawned");
    }

    void buildOrrery() {
        // Pedestal, then a spinning arm on top. The star, planet and moon hang
        // off the arm, so the arm's rotation carries them:
        //
        //   Orrery Arm   (Spin 25 deg/s)
        //   ├─ Star
        //   └─ Planet    (Spin 120 deg/s, 2.4 m out)
        //      └─ Moon   (1.8 planet-units out)
        addMesh("Pedestal", *cube_, colored(Vec3(0.35f), 0.2f), Vec3(5.5f, 0.6f, -5.0f), Vec3(0.6f, 1.2f, 0.6f));
        const Entity arm = scene_.createEntity("Orrery Arm");
        scene_.get<Transform>(arm).position = Vec3(5.5f, 2.0f, -5.0f);
        scene_.add<Spin>(arm, Vec3(0.0f, 1.0f, 0.0f), 25.0f);
        orreryArm_ = arm;

        addMesh("Star", *sphere_, colored(Vec3(1.0f, 0.75f, 0.2f), 0.8f, 16.0f), Vec3(0.0f), Vec3(1.1f), arm);
        planet_ = addMesh("Planet", *sphere_, colored(Vec3(0.2f, 0.6f, 0.9f), 0.5f, 48.0f), Vec3(2.4f, 0.0f, 0.0f),
                          Vec3(0.5f), arm);
        scene_.add<Spin>(planet_, Vec3(0.1f, 1.0f, 0.0f), 120.0f);
        addMesh("Moon", *sphere_, colored(Vec3(0.8f), 0.2f), Vec3(1.8f, 0.0f, 0.0f), Vec3(0.4f), planet_);
    }

    // ---------------------------------------------------------------- per-frame
    void followFreeCamera() {
        // The fly controller moves a plain Camera every frame; copy it onto the
        // camera entity. It moves per frame, not per fixed step, so there is
        // nothing to blend: reset the interpolation or it would lag behind.
        Transform& t = scene_.get<Transform>(cameraEntity_);
        t.position = freeCamera_.position;
        t.rotation = freeCamera_.rotation();
        t.resetInterpolation();
    }

    void moveSun(float dt) {
        const Input& in = input();
        const float turn = 1.2f * dt;
        if (in.isKeyDown(Key::J)) sunAzimuth_ += turn;
        if (in.isKeyDown(Key::L)) sunAzimuth_ -= turn;
        if (in.isKeyDown(Key::I)) sunElevation_ += turn;
        if (in.isKeyDown(Key::K)) sunElevation_ -= turn;
        sunElevation_ = Math::clamp(sunElevation_, Math::toRadians(3.0f), Math::toRadians(89.0f));
        // The light shines along the entity's -Z: turn it by the azimuth, then
        // tilt it down by the elevation.
        Transform& t = scene_.get<Transform>(sunEntity_);
        t.rotation = glm::angleAxis(sunAzimuth_, Math::WorldUp) * glm::angleAxis(-sunElevation_, Math::WorldRight);
        t.resetInterpolation();
    }

    void handleKeys() {
        const Input& in = input();
        if (in.wasKeyPressed(Key::Escape)) quit();
        if (in.wasKeyPressed(Key::R)) renderer_->reloadShaders();
        if (in.wasKeyPressed(Key::F12)) screenshotRequested_ = true;
        if (in.wasKeyPressed(Key::H)) printHelp();
        if (in.wasKeyPressed(Key::Tab)) printHierarchy();
        if (in.wasKeyPressed(Key::F1)) {
            renderer_->wireframe = !renderer_->wireframe;
            ENGINE_INFO("Wireframe ", renderer_->wireframe ? "on" : "off");
        }
        if (in.wasKeyPressed(Key::V)) {
            window().setVSync(!window().vsync());
            ENGINE_INFO("Vsync ", window().vsync() ? "on" : "off");
        }
        if (in.wasKeyPressed(Key::C)) {
            sceneRenderer_->showColliders = !sceneRenderer_->showColliders;
            ENGINE_INFO("Colliders ", sceneRenderer_->showColliders ? "shown" : "hidden");
        }
        if (in.wasKeyPressed(Key::P)) {
            paused_ = !paused_;
            ENGINE_INFO("Simulation ", paused_ ? "paused" : "running");
        }
        if (in.wasKeyPressed(Key::B)) togglePlanetParent();
        if (in.wasKeyPressed(Key::N)) spawnCrate();
        if (in.wasKeyPressed(Key::Backspace)) removeNewestCrate();
        if (in.wasKeyPressed(Key::Delete)) removeAllSpawned();
    }

    void togglePlanetParent() {
        // keepWorldTransform = true: the planet stays exactly where it is on
        // screen; only what it's attached to changes.
        const bool attached = scene_.parentOf(planet_) == orreryArm_;
        scene_.setParent(planet_, attached ? NullEntity : orreryArm_);
        ENGINE_INFO("Planet ", attached ? "detached: it stays put and stops orbiting (its moon still circles it)"
                                         : "re-attached to the orrery arm");
    }

    void spawnCrate() {
        const Vec3 spot = freeCamera_.position + freeCamera_.forward() * 3.0f;
        const Entity crate = addMesh("Spawned Crate " + std::to_string(++spawnCounter_), *cube_, crateMaterial(),
                                     Vec3(spot.x, std::max(spot.y, 0.5f), spot.z), Vec3(0.8f), spawnGroup_);
        scene_.get<Transform>(crate).rotation = glm::angleAxis(freeCamera_.yaw, Math::WorldUp);
        scene_.add<Collider>(crate);
        scene_.add<HitHighlight>(crate);
        ENGINE_INFO("Added ", scene_.nameOf(crate), " (", scene_.entityCount(), " entities)");
    }

    void removeNewestCrate() {
        const std::vector<Entity>& spawned = scene_.childrenOf(spawnGroup_);
        if (spawned.empty()) {
            ENGINE_INFO("Nothing spawned to remove (N spawns a crate)");
            return;
        }
        const Entity newest = spawned.back();
        ENGINE_INFO("Removed ", scene_.nameOf(newest));
        scene_.destroyEntity(newest);
    }

    void removeAllSpawned() {
        const size_t count = scene_.childrenOf(spawnGroup_).size();
        // Destroying the group destroys every child with it.
        scene_.destroyEntity(spawnGroup_);
        spawnGroup_ = scene_.createEntity("Spawned");
        ENGINE_INFO("Removed the Spawned group and its ", count, " crates (", scene_.entityCount(), " entities left)");
    }

    // ---------------------------------------------------------------- output
    void printHierarchy() const {
        std::ostringstream out;
        out << "Scene hierarchy (" << scene_.entityCount() << " entities):\n";
        for (Entity root : scene_.roots()) {
            appendTree(out, root, 1);
        }
        ENGINE_INFO(out.str());
    }

    void appendTree(std::ostringstream& out, Entity e, int depth) const {
        out << std::string(static_cast<size_t>(depth) * 2, ' ') << scene_.nameOf(e);
        // List the components beyond the three every entity has.
        std::vector<const char*> parts;
        if (scene_.has<MeshRenderer>(e)) parts.push_back("MeshRenderer");
        if (scene_.has<CameraComponent>(e)) parts.push_back("Camera");
        if (scene_.has<LightComponent>(e)) parts.push_back("Light");
        if (scene_.has<Collider>(e)) parts.push_back("Collider");
        if (scene_.has<Spin>(e)) parts.push_back("Spin");
        if (scene_.has<Patrol>(e)) parts.push_back("Patrol");
        if (!parts.empty()) {
            out << "  [";
            for (size_t i = 0; i < parts.size(); ++i) {
                out << (i ? ", " : "") << parts[i];
            }
            out << "]";
        }
        out << '\n';
        for (Entity child : scene_.childrenOf(e)) {
            appendTree(out, child, depth + 1);
        }
    }

    void maybeTakeScreenshot(const IVec2& fb) {
        const bool lastFrame = settings().maxFrames > 0 && stats().frame + 1 == settings().maxFrames;
        if (screenshotRequested_ || (lastFrame && !screenshotPath_.empty())) {
            saveScreenshot(screenshotPath_.empty() ? "screenshot.png" : screenshotPath_, fb.x, fb.y);
            screenshotRequested_ = false;
        }
    }

    void refreshTitle(double frameSeconds) {
        titleTimer_ += frameSeconds;
        if (titleTimer_ < 0.5) {
            return;
        }
        titleTimer_ = 0.0;
        const RenderStats& s = renderer_->stats();
        std::ostringstream title;
        title.precision(1);
        title << std::fixed << "Xordor Sandbox  |  " << stats().fps << " FPS  |  " << scene_.entityCount()
              << " entities, " << s.drawCalls << " draws  |  ball touches " << overlapCount_
              << (paused_ ? "  |  PAUSED" : "");
        window().setTitle(title.str());
    }

    static void printHelp() {
        ENGINE_INFO("Controls: right mouse + WASD/QE fly, J/L/I/K sun, C colliders, P pause, B detach/attach planet, "
                    "N spawn crate, Backspace remove newest, Delete remove all spawned, Tab print hierarchy, "
                    "R reload shaders, F1 wireframe, V vsync, F12 screenshot, Esc quits");
    }

    // GPU objects must be deleted while the OpenGL context exists. They are:
    // C++ destroys these members before the Application base class, which
    // owns the window. The scene only points at meshes and textures, so it
    // may be destroyed in any order relative to them.
    std::unique_ptr<ForwardRenderer> renderer_;
    std::unique_ptr<SceneRenderer> sceneRenderer_;
    std::unique_ptr<Mesh> cube_, plane_, sphere_;
    std::unique_ptr<Texture> crate_, ground_;
    Scene scene_;

    Entity cameraEntity_, sunEntity_, ball_, orreryArm_, planet_, spawnGroup_;
    Camera freeCamera_;
    FlyCameraController flyCamera_; // moves freeCamera_ with the mouse and keys
    float sunAzimuth_ = Math::toRadians(35.0f);
    float sunElevation_ = Math::toRadians(50.0f);

    bool paused_ = false;
    bool screenshotRequested_ = false;
    int overlapCount_ = 0;
    int spawnCounter_ = 0;
    std::string screenshotPath_;
    double titleTimer_ = 0.0;
};

bool parseOptions(int argc, char** argv, Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--frames" && hasValue) {
            options.frames = std::atoll(argv[++i]);
        } else if (arg == "--screenshot" && hasValue) {
            options.screenshotPath = argv[++i];
        } else if (arg == "--no-vsync") {
            options.vsync = false;
        } else {
            ENGINE_ERROR("Unknown option ", arg, ". Options: --frames N, --no-vsync, --screenshot FILE.png");
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parseOptions(argc, argv, options)) {
        return 2;
    }
    try {
        ApplicationSettings settings;
        settings.window.title = "Xordor Sandbox";
        settings.window.vsync = options.vsync;
        settings.maxFrames = options.frames;
        settings.logFile = "sandbox.log";
        Sandbox app(settings, options.screenshotPath);
        return app.run();
    } catch (const std::exception& error) {
        ENGINE_ERROR("Fatal: ", error.what());
        return 1;
    }
}
