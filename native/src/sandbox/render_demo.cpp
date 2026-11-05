// Render demo (Phase 2): a small lit 3D scene, drawn straight with the renderer (no entities).
//
// Shows every part of the renderer: meshes made in code (cube, plane, sphere),
// a texture loaded from disk and one generated in memory, a fly camera, and one
// directional light (the sun) with Blinn-Phong shading.
//
// What to look at:
//   * Hold the right mouse button and use WASD to fly, Q/E to go down/up, Shift
//     to go faster, and the scroll wheel to change speed.
//   * J/L turn the sun around, I/K raise and lower it. Watch the bright side of
//     each crate follow it, and the highlights on the spheres move.
//   * The five spheres go from dull (shininess 8) to sharp (shininess 128).
//   * The white egg is a sphere scaled to (1.6, 0.6, 1). Its lighting is right
//     because of the normal matrix (see Math::normalMatrix); the flat plank
//     next to the crates is a scaled cube too.
//   * The spinning crate turns in onFixedUpdate at 60 Hz; onRender blends its
//     last two rotations (slerp), so it stays smooth at any frame rate.
//   * Edit assets/shaders/lit.frag while this runs and save: the change shows up
//     within half a second. Break it on purpose: the error is logged with the
//     failing line, and the last good shader stays in use. R reloads by hand.
//   * F1 wireframe, V vsync, F12 screenshot, H help, Esc quits.

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

#include <cmath>
#include <cstdlib>
#include <exception>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace Engine;

namespace {

// Display-space (sRGB) sky color for the background.
const Vec3 SkyColor{0.47f, 0.62f, 0.80f};

struct Options {
    long long frames = 0;
    bool vsync = true;
    std::string screenshotPath;
};

// One thing in the scene. Phase 3 replaces this with entities and components.
struct Prop {
    const Mesh* mesh;
    Material material;
    Vec3 position{0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f};
};

class RenderDemo final : public Application {
public:
    RenderDemo(ApplicationSettings settings, std::string screenshotPath)
        : Application(std::move(settings)), screenshotPath_(std::move(screenshotPath)) {}

protected:
    void onStart() override {
        // The renderer and all GPU resources need the OpenGL context, which
        // exists once the window does, so they are created here and not in the
        // constructor's initializer list.
        renderer_ = std::make_unique<ForwardRenderer>();
        const IVec2 fb = window().framebufferSize();
        renderer_->setViewport(fb.x, fb.y);

        loadResources();
        buildScene();
        camera_.position = Vec3(0.0f, 3.0f, 9.0f);
        camera_.lookAt(Vec3(0.0f, 0.8f, 0.0f));
        printHelp();
    }

    void onFixedUpdate(double dtSeconds) override {
        // The spinning crate: 90 degrees per second around a tilted axis.
        previousSpin_ = spin_;
        const Quat step = glm::angleAxis(Math::HalfPi * static_cast<float>(dtSeconds),
                                         glm::normalize(Vec3(0.3f, 1.0f, 0.2f)));
        // Re-normalizing stops tiny rounding errors from piling up over
        // thousands of multiplications and slowly scaling the cube.
        spin_ = glm::normalize(step * spin_);
    }

    void onUpdate(double frameSeconds) override {
        const float dt = static_cast<float>(frameSeconds);
        flyCamera_.update(camera_, input(), dt);
        if (flyCamera_.wantsCursorCaptured() != window().cursorCaptured()) {
            window().setCursorCaptured(flyCamera_.wantsCursorCaptured());
        }
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
        camera_.aspect = static_cast<float>(fb.x) / static_cast<float>(fb.y);

        renderer_->beginFrame(camera_, sun_, SkyColor);
        for (const Prop& prop : props_) {
            renderer_->submit(*prop.mesh, prop.material, Math::composeTRS(prop.position, prop.rotation, prop.scale));
        }
        // Draw the spinning crate between its last two simulated rotations.
        const Quat spin = glm::slerp(previousSpin_, spin_, static_cast<float>(alpha));
        renderer_->submit(*cube_, crateMaterial_, Math::composeTRS(Vec3(2.5f, 1.4f, -1.0f), spin, Vec3(1.2f)));
        renderer_->endFrame();

        maybeTakeScreenshot(fb);
    }

    void onFramebufferResize(int width, int height) override {
        // run() reports the first size before onStart, when there's no renderer yet.
        if (renderer_) {
            renderer_->setViewport(width, height);
        }
    }

private:
    // ---------------------------------------------------------------- setup
    void loadResources() {
        cube_ = std::make_unique<Mesh>(Primitives::cube());
        plane_ = std::make_unique<Mesh>(Primitives::plane());
        sphere_ = std::make_unique<Mesh>(Primitives::sphere(0.5f, 48, 24));

        std::string error;
        if (auto texture = Texture::fromFile(assetPath("textures/crate.png"), {}, &error)) {
            crate_ = std::make_unique<Texture>(std::move(*texture));
        } else {
            // Not fatal: the scene still makes sense with a stand-in texture,
            // but say so loudly instead of quietly drawing something else.
            ENGINE_WARN("Using a checkerboard instead of the crate texture: ", error);
            crate_ = std::make_unique<Texture>(makeCheckerboard(64, 4, Vec3(0.8f, 0.5f, 0.2f), Vec3(0.4f, 0.2f, 0.1f)));
        }
        // An image made in memory: a 1 m checkerboard for the ground.
        ground_ = std::make_unique<Texture>(makeCheckerboard(256, 8, Vec3(0.55f), Vec3(0.42f)));
    }

    void buildScene() {
        crateMaterial_.albedo = crate_.get();
        crateMaterial_.specular = 0.1f;
        crateMaterial_.shininess = 16.0f;

        Material ground;
        ground.albedo = ground_.get();
        // 200 m wide: big enough that its edge is hard to spot at the horizon.
        // TODO(later phase): fog or a sky dome would hide the edge for good.
        // 25 repeats of an 8-cell image -> 1 m squares.
        ground.uvScale = Vec2(25.0f);
        ground.specular = 0.05f;
        props_.push_back({plane_.get(), ground, Vec3(0.0f), Quat(1, 0, 0, 0), Vec3(200.0f, 1.0f, 200.0f)});

        // A stack of crates. The cube is 1 m, centered, so y = 0.5 sits on the ground.
        addCrate(Vec3(-3.0f, 0.5f, -2.0f), 0.0f);
        addCrate(Vec3(-1.8f, 0.5f, -2.3f), 20.0f);
        addCrate(Vec3(-2.4f, 1.5f, -2.1f), -12.0f);
        // Non-uniform scale: a flat plank lying on the ground.
        props_.push_back({cube_.get(), crateMaterial_, Vec3(-2.3f, 0.125f, 0.4f),
                          Math::quatFromEulerDegrees(Vec3(0.0f, 25.0f, 0.0f)), Vec3(3.0f, 0.25f, 1.0f)});

        // Spheres from dull to shiny.
        const Vec3 colors[] = {{0.85f, 0.20f, 0.20f}, {0.95f, 0.60f, 0.15f}, {0.30f, 0.75f, 0.30f},
                               {0.20f, 0.45f, 0.90f}, {0.65f, 0.30f, 0.85f}};
        const float shininess[] = {8.0f, 16.0f, 32.0f, 64.0f, 128.0f};
        for (int i = 0; i < 5; ++i) {
            Material m;
            m.baseColor = colors[i];
            m.specular = 0.6f;
            m.shininess = shininess[i];
            props_.push_back({sphere_.get(), m, Vec3(-3.0f + 1.5f * static_cast<float>(i), 0.5f, 2.2f),
                              Quat(1, 0, 0, 0), Vec3(1.0f)});
        }

        // A squashed sphere: non-uniform scale with slanted normals, the case
        // the normal matrix exists for. Its highlight sits where it should.
        Material egg;
        egg.baseColor = Vec3(0.9f);
        egg.specular = 0.6f;
        egg.shininess = 48.0f;
        props_.push_back({sphere_.get(), egg, Vec3(5.2f, 0.3f, 1.0f), Math::quatFromEulerDegrees(Vec3(0, -30, 0)),
                          Vec3(1.6f, 0.6f, 1.0f)});
    }

    void addCrate(const Vec3& position, float yawDegrees) {
        props_.push_back({cube_.get(), crateMaterial_, position,
                          Math::quatFromEulerDegrees(Vec3(0.0f, yawDegrees, 0.0f)), Vec3(1.0f)});
    }

    // ---------------------------------------------------------------- input
    void moveSun(float dt) {
        const Input& in = input();
        const float turn = 1.2f * dt; // radians per second
        if (in.isKeyDown(Key::J)) sunAzimuth_ += turn;
        if (in.isKeyDown(Key::L)) sunAzimuth_ -= turn;
        if (in.isKeyDown(Key::I)) sunElevation_ += turn;
        if (in.isKeyDown(Key::K)) sunElevation_ -= turn;
        // Below 3 degrees the ground would be lit from underneath at a grazing angle.
        sunElevation_ = Math::clamp(sunElevation_, Math::toRadians(3.0f), Math::toRadians(89.0f));
        // Spherical to Cartesian: the point on the sky where the sun is...
        const Vec3 towardSun(std::cos(sunElevation_) * std::sin(sunAzimuth_), std::sin(sunElevation_),
                             std::cos(sunElevation_) * std::cos(sunAzimuth_));
        // ...and the light travels the other way, from the sun down to the scene.
        sun_.direction = -towardSun;
    }

    void handleKeys() {
        const Input& in = input();
        if (in.wasKeyPressed(Key::Escape)) {
            quit();
        }
        if (in.wasKeyPressed(Key::R)) {
            renderer_->reloadShaders();
        }
        if (in.wasKeyPressed(Key::F1)) {
            renderer_->wireframe = !renderer_->wireframe;
            ENGINE_INFO("Wireframe ", renderer_->wireframe ? "on" : "off");
        }
        if (in.wasKeyPressed(Key::V)) {
            window().setVSync(!window().vsync());
            ENGINE_INFO("Vsync ", window().vsync() ? "on" : "off");
        }
        if (in.wasKeyPressed(Key::F12)) {
            screenshotRequested_ = true;
        }
        if (in.wasKeyPressed(Key::H)) {
            printHelp();
        }
    }

    // ---------------------------------------------------------------- output
    void maybeTakeScreenshot(const IVec2& fb) {
        // --screenshot saves the very last frame of a --frames run.
        const bool lastFrame = settings().maxFrames > 0 && stats().frame + 1 == settings().maxFrames;
        if (screenshotRequested_ || (lastFrame && !screenshotPath_.empty())) {
            const std::string path = screenshotPath_.empty() ? "screenshot.png" : screenshotPath_;
            saveScreenshot(path, fb.x, fb.y);
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
        title << std::fixed << "Xordor Render Demo  |  " << stats().fps << " FPS  " << stats().frameMs << " ms  |  "
              << s.drawCalls << " draws, " << s.triangles << " triangles  |  camera " << flyCamera_.speed << " m/s";
        window().setTitle(title.str());
    }

    static void printHelp() {
        ENGINE_INFO("Controls: hold right mouse + WASD/QE to fly (Shift fast, scroll = speed), "
                    "J/L/I/K move the sun, R reload shaders, F1 wireframe, V vsync, F12 screenshot, Esc quits");
    }

    // GPU objects must be deleted while the OpenGL context exists. It does here:
    // C++ destroys these members before the Application base class, which
    // owns the window (and with it the context).
    std::unique_ptr<ForwardRenderer> renderer_;
    std::unique_ptr<Mesh> cube_, plane_, sphere_;
    std::unique_ptr<Texture> crate_, ground_;
    Material crateMaterial_;
    std::vector<Prop> props_;

    Camera camera_;
    FlyCameraController flyCamera_;
    DirectionalLight sun_;
    float sunAzimuth_ = Math::toRadians(35.0f);
    float sunElevation_ = Math::toRadians(50.0f);
    Quat spin_{1.0f, 0.0f, 0.0f, 0.0f};
    Quat previousSpin_{1.0f, 0.0f, 0.0f, 0.0f};

    bool screenshotRequested_ = false;
    std::string screenshotPath_;
    double titleTimer_ = 0.0;
};

// Tiny command-line parser: --frames N, --no-vsync, --screenshot FILE.
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
    // Everything that can fail (no display, no OpenGL 4.1 driver, missing
    // shaders) throws from a constructor. Catching here turns that into a
    // readable message and an exit code instead of a crash.
    try {
        ApplicationSettings settings;
        settings.window.title = "Xordor Render Demo";
        settings.window.vsync = options.vsync;
        settings.maxFrames = options.frames;
        settings.logFile = "render_demo.log";
        RenderDemo app(settings, options.screenshotPath);
        return app.run();
    } catch (const std::exception& error) {
        ENGINE_ERROR("Fatal: ", error.what());
        return 1;
    }
}
