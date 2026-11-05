// Sandbox, Phase 4: the entity scene, now made of asset files and saved as JSON.
//
// At start the sandbox scans the assets folder (giving every file an id in a
// .meta file), then loads assets/scenes/sandbox.scene.json. If that file is
// missing it builds the scene in code and saves it, so the file always exists
// to open in a text editor. Meshes and textures are referred to by asset id
// and loaded the first time they're drawn: the barrel is an OBJ file, the
// rock a glTF file, and every texture a PNG.
//
// What to try:
//   * F5 saves the scene, F9 loads it back. Move things (B, N, Delete...),
//     save, quit, start again: they're where you left them.
//   * Edit the scene file while the sandbox runs (change a crate's
//     "position", or the "baseColor" of the ball), save it, press F9.
//   * Break the file on purpose (delete a comma) and press F9: the error
//     names the line, and the scene on screen stays as it was.
//   * Tab prints the hierarchy, A prints the asset list with ids.
//   * The Phase 3 controls still work: C colliders, P pause, B detach the
//     planet, N / Backspace / Delete add and remove crates, right mouse +
//     WASD to fly, J/L/I/K sun, R shaders, F1 wireframe, F12 screenshot, Esc.

#include "assets/AssetManager.h"
#include "assets/AssetRegistry.h"
#include "assets/SceneSerializer.h"
#include "core/Application.h"
#include "core/Log.h"
#include "core/Math.h"
#include "core/Paths.h"
#include "core/Screenshot.h"
#include "render/Camera.h"
#include "render/FlyCameraController.h"
#include "render/ForwardRenderer.h"
#include "scene/Collision.h"
#include "scene/Scene.h"
#include "scene/SceneRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace Engine;

namespace {

const char* ScenePath = "scenes/sandbox.scene.json"; // relative to the assets folder

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

// Teaches the serializer the three components above, so they're saved too.
void registerSandboxComponents(SceneSerializer& serializer) {
    serializer.registerComponent<Spin>(
        "spin", [](const Spin& s) { return Json{{"axis", jsonArray(s.axis)}, {"degreesPerSecond", tidy(s.degreesPerSecond)}}; },
        [](Spin& s, const Json& j) {
            s.axis = readVec3(j, "axis", Vec3(0.0f, 1.0f, 0.0f));
            s.degreesPerSecond = readFloat(j, "degreesPerSecond", 45.0f);
        });
    serializer.registerComponent<Patrol>(
        "patrol", [](const Patrol& p) { return Json{{"minX", tidy(p.minX)}, {"maxX", tidy(p.maxX)}, {"speed", tidy(p.speed)}}; },
        [](Patrol& p, const Json& j) {
            p.minX = readFloat(j, "minX", -5.0f);
            p.maxX = readFloat(j, "maxX", 5.0f);
            p.speed = readFloat(j, "speed", 2.0f);
        });
    serializer.registerComponent<HitHighlight>(
        "hitHighlight",
        [](const HitHighlight& h) { return Json{{"normalColor", jsonArray(h.normalColor)}}; },
        [](HitHighlight& h, const Json& j) { h.normalColor = readVec3(j, "normalColor", Vec3(1.0f)); });
}

struct Options {
    long long frames = 0;
    bool vsync = true;
    bool rebuildScene = false;
    std::string screenshotPath;
};

class Sandbox final : public Application {
public:
    Sandbox(ApplicationSettings settings, Options options)
        : Application(std::move(settings)), options_(std::move(options)), registry_(assetDirectory()),
          serializer_(&registry_) {
        registerSandboxComponents(serializer_);
    }

protected:
    void onStart() override {
        renderer_ = std::make_unique<ForwardRenderer>();
        sceneRenderer_ = std::make_unique<SceneRenderer>(*renderer_);
        const IVec2 fb = window().framebufferSize();
        renderer_->setViewport(fb.x, fb.y);

        registry_.scan();
        assets_ = std::make_unique<AssetManager>(registry_);

        if (options_.rebuildScene || !loadSceneFile()) {
            ENGINE_INFO("Building the default scene in code");
            buildDefaultScene();
            findSceneEntities(); // the same set-up as after loading a file
            saveSceneFile();
        }
        printHelp();
    }

    void onFixedUpdate(double dtSeconds) override {
        scene_.beginFixedStep(); // first, always: lets rendering blend toward this step
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
        sceneRenderer_->render(scene_, *assets_, static_cast<float>(fb.x) / static_cast<float>(fb.y),
                               static_cast<float>(alpha));
        maybeTakeScreenshot(fb);
    }

    void onFramebufferResize(int width, int height) override {
        if (renderer_) {
            renderer_->setViewport(width, height);
        }
    }

private:
    // ---------------------------------------------------------------- systems
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
                p.speed = -p.speed;
            }
        });
    }

    void hitHighlightSystem() {
        // Every entity with a Patrol is a "ball" that lights up what it touches.
        std::vector<Entity> hits;
        scene_.registry().each<Patrol>([&](Entity e, Patrol&) {
            const std::vector<Entity> touching = overlapping(scene_, e);
            hits.insert(hits.end(), touching.begin(), touching.end());
        });
        overlapCount_ = static_cast<int>(hits.size());
        scene_.registry().each<HitHighlight, MeshRenderer>([&](Entity e, HitHighlight& h, MeshRenderer& mr) {
            const bool hit = std::find(hits.begin(), hits.end(), e) != hits.end();
            mr.material.baseColor = hit ? Vec3(1.0f, 0.25f, 0.2f) : h.normalColor;
        });
    }

    // ---------------------------------------------------------------- scene files
    std::string scenePath() const { return registry_.root() + "/" + ScenePath; }

    bool loadSceneFile() {
        if (!std::filesystem::exists(scenePath())) {
            ENGINE_INFO("No scene file yet at ", scenePath());
            return false;
        }
        std::string error;
        if (!serializer_.load(scenePath(), scene_, &error)) {
            // The scene on screen (if any) is unchanged: load() only replaces
            // it when the whole file was valid.
            ENGINE_ERROR("Could not load the scene: ", error);
            return false;
        }
        ENGINE_INFO("Loaded ", ScenePath, " (", scene_.entityCount(), " entities)");
        findSceneEntities();
        return true;
    }

    void saveSceneFile() {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(scenePath()).parent_path(), ec);
        std::string error;
        if (!serializer_.save(scene_, scenePath(), &error)) {
            ENGINE_ERROR("Could not save the scene: ", error);
            return;
        }
        // A new file becomes an asset with an id, like any other.
        registry_.registerFile(ScenePath);
        ENGINE_INFO("Saved ", ScenePath, " (", scene_.entityCount(), " entities)");
    }

    // A loaded scene is new data: find the entities the sandbox drives by
    // name, and line the free camera and sun controls up with the file.
    void findSceneEntities() {
        cameraEntity_ = scene_.primaryCamera();
        sunEntity_ = scene_.findByName("Sun");
        orreryArm_ = scene_.findByName("Orrery Arm");
        planet_ = scene_.findByName("Planet");
        spawnGroup_ = scene_.findByName("Spawned");
        if (cameraEntity_.isNull()) {
            ENGINE_WARN("The scene has no camera; adding one");
            cameraEntity_ = scene_.createEntity("Main Camera");
            scene_.add<CameraComponent>(cameraEntity_);
        }
        if (spawnGroup_.isNull()) {
            spawnGroup_ = scene_.createEntity("Spawned");
        }
        const Transform& cam = scene_.get<Transform>(cameraEntity_);
        freeCamera_.position = cam.position;
        freeCamera_.lookAt(cam.position + cam.rotation * Math::WorldForward);
        if (!sunEntity_.isNull()) {
            // Undo moveSun's rotation = yaw(azimuth) * pitch(-elevation) from the
            // direction the light shines.
            const Vec3 dir = scene_.get<Transform>(sunEntity_).rotation * Math::WorldForward;
            sunElevation_ = std::asin(Math::clamp(-dir.y, -1.0f, 1.0f));
            sunAzimuth_ = std::atan2(-dir.x, -dir.z);
        }
    }

    // ---------------------------------------------------------------- the default scene
    AssetId asset(const std::string& path) const {
        const AssetId id = registry_.idForPath(path);
        if (id.isNull()) {
            ENGINE_WARN("Asset ", path, " is missing from ", registry_.root(), "; that part of the scene will be missing too");
        }
        return id;
    }

    Entity addMesh(const std::string& name, AssetId mesh, const MeshMaterial& material, const Vec3& position,
                   const Vec3& scale = Vec3(1.0f), Entity parent = NullEntity) {
        const Entity e = scene_.createEntity(name);
        if (!parent.isNull()) {
            scene_.setParent(e, parent, false);
        }
        Transform& t = scene_.get<Transform>(e);
        t.position = position;
        t.scale = scale;
        MeshRenderer& mr = scene_.add<MeshRenderer>(e);
        mr.mesh = mesh;
        mr.material = material;
        return e;
    }

    MeshMaterial textured(const std::string& texturePath, float specular = 0.1f, float shininess = 16.0f) const {
        MeshMaterial m;
        m.albedo = asset(texturePath);
        m.specular = specular;
        m.shininess = shininess;
        return m;
    }

    static MeshMaterial colored(const Vec3& color, float specular = 0.4f, float shininess = 32.0f) {
        MeshMaterial m;
        m.baseColor = color;
        m.specular = specular;
        m.shininess = shininess;
        return m;
    }

    void buildDefaultScene() {
        scene_ = Scene();
        cameraEntity_ = scene_.createEntity("Main Camera");
        scene_.add<CameraComponent>(cameraEntity_);
        Transform& cam = scene_.get<Transform>(cameraEntity_);
        cam.position = Vec3(1.0f, 3.0f, 7.0f);
        Camera aim;
        aim.position = cam.position;
        aim.lookAt(Vec3(1.8f, 0.9f, -2.5f));
        cam.rotation = aim.rotation();

        sunEntity_ = scene_.createEntity("Sun");
        scene_.add<LightComponent>(sunEntity_);
        // Turned by the azimuth, tilted down by the elevation (as moveSun does).
        scene_.get<Transform>(sunEntity_).rotation =
            glm::angleAxis(Math::toRadians(35.0f), Math::WorldUp) * glm::angleAxis(-Math::toRadians(50.0f), Math::WorldRight);

        MeshMaterial ground = textured("textures/checker.png", 0.05f);
        ground.uvScale = Vec2(25.0f); // 200 m, 8 squares per repeat -> 1 m squares
        addMesh("Ground", BuiltinAssets::Plane, ground, Vec3(0.0f), Vec3(200.0f, 1.0f, 200.0f));

        const Entity row = scene_.createEntity("Crate Row");
        for (int i = 0; i < 3; ++i) {
            const Entity crate = addMesh("Crate " + std::to_string(i + 1), BuiltinAssets::Cube, textured("textures/crate.png"),
                                         Vec3(-3.0f + 3.0f * static_cast<float>(i), 0.5f, -1.0f), Vec3(1.0f), row);
            scene_.add<Collider>(crate);
            scene_.add<HitHighlight>(crate);
        }

        const Entity ball = addMesh("Ball", BuiltinAssets::Sphere, colored(Vec3(0.25f, 0.5f, 1.0f), 0.6f, 64.0f),
                                    Vec3(-5.0f, 0.5f, -0.4f), Vec3(0.7f));
        scene_.add<Collider>(ball).shape = ColliderShape::Sphere;
        scene_.add<Patrol>(ball, -5.0f, 5.0f, 2.0f);

        // Models from files: an OBJ barrel (its origin is at its base) and a glTF rock.
        const Entity props = scene_.createEntity("Props");
        for (int i = 0; i < 2; ++i) {
            const Entity barrel = addMesh("Barrel " + std::to_string(i + 1), asset("models/barrel.obj"),
                                          textured("textures/barrel.png", 0.2f, 24.0f),
                                          Vec3(-4.4f + 0.85f * static_cast<float>(i), 0.0f, 0.9f), Vec3(1.0f), props);
            scene_.get<Transform>(barrel).rotation = Math::quatFromEulerDegrees(Vec3(0.0f, 40.0f * static_cast<float>(i), 0.0f));
            Collider& c = scene_.add<Collider>(barrel);
            c.center = Vec3(0.0f, 0.5f, 0.0f);
            c.halfExtents = Vec3(0.4f, 0.5f, 0.4f);
        }
        const Entity rock = addMesh("Rock", asset("models/rock.gltf"), colored(Vec3(0.33f, 0.31f, 0.28f), 0.05f, 8.0f),
                                    Vec3(2.4f, 0.25f, 1.8f), Vec3(1.4f), props);
        scene_.get<Transform>(rock).rotation = Math::quatFromEulerDegrees(Vec3(0.0f, 30.0f, 0.0f));

        buildOrrery();
        spawnGroup_ = scene_.createEntity("Spawned");
    }

    void buildOrrery() {
        addMesh("Pedestal", BuiltinAssets::Cube, colored(Vec3(0.35f), 0.2f), Vec3(5.5f, 0.6f, -5.0f), Vec3(0.6f, 1.2f, 0.6f));
        orreryArm_ = scene_.createEntity("Orrery Arm");
        scene_.get<Transform>(orreryArm_).position = Vec3(5.5f, 2.0f, -5.0f);
        scene_.add<Spin>(orreryArm_, Vec3(0.0f, 1.0f, 0.0f), 25.0f);
        addMesh("Star", BuiltinAssets::Sphere, colored(Vec3(1.0f, 0.75f, 0.2f), 0.8f, 16.0f), Vec3(0.0f), Vec3(1.1f), orreryArm_);
        planet_ = addMesh("Planet", BuiltinAssets::Sphere, colored(Vec3(0.2f, 0.6f, 0.9f), 0.5f, 48.0f), Vec3(2.4f, 0.0f, 0.0f),
                          Vec3(0.5f), orreryArm_);
        scene_.add<Spin>(planet_, Vec3(0.1f, 1.0f, 0.0f), 120.0f);
        addMesh("Moon", BuiltinAssets::Sphere, colored(Vec3(0.8f), 0.2f), Vec3(1.8f, 0.0f, 0.0f), Vec3(0.4f), planet_);
    }

    // ---------------------------------------------------------------- per-frame
    void followFreeCamera() {
        Transform& t = scene_.get<Transform>(cameraEntity_);
        t.position = freeCamera_.position;
        t.rotation = freeCamera_.rotation();
        t.resetInterpolation(); // moved per frame: nothing to blend
    }

    void moveSun(float dt) {
        if (sunEntity_.isNull() || !scene_.isAlive(sunEntity_)) {
            return;
        }
        const Input& in = input();
        const float turn = 1.2f * dt;
        if (in.isKeyDown(Key::J)) sunAzimuth_ += turn;
        if (in.isKeyDown(Key::L)) sunAzimuth_ -= turn;
        if (in.isKeyDown(Key::I)) sunElevation_ += turn;
        if (in.isKeyDown(Key::K)) sunElevation_ -= turn;
        sunElevation_ = Math::clamp(sunElevation_, Math::toRadians(3.0f), Math::toRadians(89.0f));
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
        if (in.wasKeyPressed(Key::A) && !flyCamera_.wantsCursorCaptured()) printAssets(); // A also strafes while flying
        if (in.wasKeyPressed(Key::F5)) saveSceneFile();
        if (in.wasKeyPressed(Key::F9)) loadSceneFile();
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
        if (!scene_.isAlive(planet_) || !scene_.isAlive(orreryArm_)) {
            ENGINE_INFO("This scene has no Planet and Orrery Arm to detach");
            return;
        }
        const bool attached = scene_.parentOf(planet_) == orreryArm_;
        scene_.setParent(planet_, attached ? NullEntity : orreryArm_);
        ENGINE_INFO("Planet ", attached ? "detached: it stays put and stops orbiting" : "re-attached to the orrery arm");
    }

    void spawnCrate() {
        const Vec3 spot = freeCamera_.position + freeCamera_.forward() * 3.0f;
        const Entity crate = addMesh("Spawned Crate " + std::to_string(++spawnCounter_), BuiltinAssets::Cube,
                                     textured("textures/crate.png"), Vec3(spot.x, std::max(spot.y, 0.5f), spot.z),
                                     Vec3(0.8f), spawnGroup_);
        scene_.get<Transform>(crate).rotation = glm::angleAxis(freeCamera_.yaw, Math::WorldUp);
        scene_.add<Collider>(crate);
        scene_.add<HitHighlight>(crate);
        ENGINE_INFO("Added ", scene_.nameOf(crate), " (", scene_.entityCount(), " entities; F5 saves)");
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
        scene_.destroyEntity(spawnGroup_);
        spawnGroup_ = scene_.createEntity("Spawned");
        ENGINE_INFO("Removed the Spawned group and its ", count, " crates");
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
        if (const MeshRenderer* mr = scene_.tryGet<MeshRenderer>(e)) {
            const AssetRecord* record = registry_.find(mr->mesh);
            out << "  [mesh " << (record ? record->path : mr->mesh == BuiltinAssets::Cube ? "cube"
                                                      : mr->mesh == BuiltinAssets::Sphere ? "sphere"
                                                      : mr->mesh == BuiltinAssets::Plane  ? "plane"
                                                                                          : "?")
                << "]";
        }
        out << '\n';
        for (Entity child : scene_.childrenOf(e)) {
            appendTree(out, child, depth + 1);
        }
    }

    void printAssets() const {
        std::ostringstream out;
        out << "Assets in " << registry_.root() << ":\n";
        for (const AssetRecord* record : registry_.all()) {
            out << "  " << record->id.toString() << "  " << AssetRegistry::typeName(record->type) << "  " << record->path
                << '\n';
        }
        out << "Loaded: " << assets_->loadedMeshes() << " meshes, " << assets_->loadedTextures() << " textures";
        ENGINE_INFO(out.str());
    }

    void maybeTakeScreenshot(const IVec2& fb) {
        const bool lastFrame = settings().maxFrames > 0 && stats().frame + 1 == settings().maxFrames;
        if (screenshotRequested_ || (lastFrame && !options_.screenshotPath.empty())) {
            saveScreenshot(options_.screenshotPath.empty() ? "screenshot.png" : options_.screenshotPath, fb.x, fb.y);
            screenshotRequested_ = false;
        }
    }

    void refreshTitle(double frameSeconds) {
        titleTimer_ += frameSeconds;
        if (titleTimer_ < 0.5) {
            return;
        }
        titleTimer_ = 0.0;
        std::ostringstream title;
        title.precision(1);
        title << std::fixed << "Xordor Sandbox  |  " << stats().fps << " FPS  |  " << scene_.entityCount() << " entities, "
              << renderer_->stats().drawCalls << " draws  |  " << ScenePath << (paused_ ? "  |  PAUSED" : "");
        window().setTitle(title.str());
    }

    static void printHelp() {
        ENGINE_INFO("Controls: F5 save scene, F9 load scene, Tab hierarchy, A assets, right mouse + WASD/QE fly, "
                    "J/L/I/K sun, C colliders, P pause, B planet, N / Backspace / Delete crates, R shaders, "
                    "F1 wireframe, F12 screenshot, Esc quits");
    }

    Options options_;
    AssetRegistry registry_;
    SceneSerializer serializer_;
    // GPU objects must be deleted while the OpenGL context exists. They are:
    // C++ destroys these members before the Application base class, which
    // owns the window.
    std::unique_ptr<ForwardRenderer> renderer_;
    std::unique_ptr<SceneRenderer> sceneRenderer_;
    std::unique_ptr<AssetManager> assets_;
    Scene scene_;

    Entity cameraEntity_, sunEntity_, orreryArm_, planet_, spawnGroup_;
    Camera freeCamera_;
    FlyCameraController flyCamera_;
    float sunAzimuth_ = Math::toRadians(35.0f);
    float sunElevation_ = Math::toRadians(50.0f);

    bool paused_ = false;
    bool screenshotRequested_ = false;
    int overlapCount_ = 0;
    int spawnCounter_ = 0;
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
        } else if (arg == "--rebuild-scene") {
            options.rebuildScene = true;
        } else {
            ENGINE_ERROR("Unknown option ", arg,
                         ". Options: --frames N, --no-vsync, --screenshot FILE.png, --rebuild-scene");
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
        Sandbox app(settings, options);
        return app.run();
    } catch (const std::exception& error) {
        ENGINE_ERROR("Fatal: ", error.what());
        return 1;
    }
}
