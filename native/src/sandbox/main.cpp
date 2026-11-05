// Sandbox, Phase 1: makes the main loop visible.
//
// There is no renderer yet, so everything is drawn with one trick: glScissor
// limits drawing to a rectangle, and glClear fills it. That draws solid boxes
// with no shaders at all.
//
// What to look at:
//   * The orange ball is simulated in onFixedUpdate at a fixed rate. Press 1-4
//     to pick 10, 30, 60 or 120 Hz. At 10 Hz, press I to switch interpolation
//     off: the ball jumps 10 times a second. Switch it back on: it moves
//     smoothly again, although the simulation still runs only 10 times a second.
//   * The blue player moves with WASD or the arrow keys, also in onFixedUpdate,
//     so it moves the same distance per second at any frame rate.
//   * Space jumps. The press is seen in onUpdate (once per frame) and remembered
//     until the next fixed step uses it, so a quick tap is never lost.
//   * The background color follows real time in onUpdate.
//   * Click to move the ball to the mouse. Scroll to resize it.
//   * V toggles vsync, F12 saves a screenshot, H prints help, Esc quits.

#include "core/Application.h"
#include "core/Log.h"
#include "core/Math.h"
#include "core/Screenshot.h"

#include <glad/gl.h>

#include <cmath>
#include <cstdlib>
#include <exception>
#include <sstream>
#include <string>
#include <utility>

using namespace Engine;

namespace {

// The demo world is 16 x 9 meters, stretched over the whole window.
constexpr float WorldWidth = 16.0f;
constexpr float WorldHeight = 9.0f;
constexpr float GroundHeight = 0.6f;
constexpr float Gravity = -18.0f; // m/s^2, stronger than Earth's: games feel floaty otherwise

struct Body {
    Vec2 position{0.0f};
    Vec2 previous{0.0f}; // position at the previous fixed step, for interpolation
    Vec2 velocity{0.0f};
    Vec2 size{1.0f};
};

struct Options {
    long long frames = 0;
    double fixedHz = 60.0;
    bool vsync = true;
    std::string screenshotPath;
};

class Sandbox final : public Application {
public:
    Sandbox(ApplicationSettings settings, std::string screenshotPath)
        : Application(std::move(settings)), screenshotPath_(std::move(screenshotPath)) {}

protected:
    void onStart() override {
        ball_.position = ball_.previous = Vec2(4.0f, 6.0f);
        ball_.velocity = Vec2(5.0f, 0.0f);
        ball_.size = Vec2(0.8f);
        player_.position = player_.previous = Vec2(8.0f, GroundHeight);
        player_.size = Vec2(0.9f, 1.4f);
        printHelp();
    }

    // Runs at a fixed rate: everything here behaves the same at any frame rate.
    void onFixedUpdate(double dtSeconds) override {
        const float dt = static_cast<float>(dtSeconds);
        simulateBall(dt);
        simulatePlayer(dt);
    }

    // Runs once per frame with the real frame time.
    void onUpdate(double frameSeconds) override {
        backgroundPhase_ += static_cast<float>(frameSeconds) * 0.25f;
        handleKeys();
        handleMouse();
        refreshTitle(frameSeconds);
    }

    void onRender(double alpha) override {
        const IVec2 fb = window().framebufferSize();
        if (fb.x == 0 || fb.y == 0) {
            return; // minimized
        }
        glDisable(GL_SCISSOR_TEST);
        clearTo(backgroundColor());
        glEnable(GL_SCISSOR_TEST);

        fillRect(Vec2(0.0f), Vec2(WorldWidth, GroundHeight), Vec3(0.20f, 0.18f, 0.24f));
        drawTimelineMarkers();

        // Interpolation: draw where the body was a fraction `alpha` of the way
        // from its previous fixed-step position to its current one. That lags a
        // tiny bit (less than one step) but turns 10 jumps a second into smooth motion.
        const float a = interpolate_ ? static_cast<float>(alpha) : 1.0f;
        fillRect(Math::lerp(player_.previous, player_.position, a), player_.size, Vec3(0.25f, 0.62f, 1.0f));
        fillRect(Math::lerp(ball_.previous, ball_.position, a), ball_.size, Vec3(1.0f, 0.62f, 0.18f));

        glDisable(GL_SCISSOR_TEST);
        maybeTakeScreenshot(fb);
    }

    void onShutdown() override {
        ENGINE_INFO("Sandbox done. Ball bounced ", bounces_, " times");
    }

private:
    // ---------------------------------------------------------------- simulation
    void simulateBall(float dt) {
        ball_.previous = ball_.position;
        ball_.velocity.y += Gravity * dt;
        ball_.position += ball_.velocity * dt;
        // Bounce off the floor and the side walls, losing a little energy.
        if (ball_.position.y < GroundHeight) {
            ball_.position.y = GroundHeight;
            ball_.velocity.y = std::fabs(ball_.velocity.y) * 0.92f;
            if (ball_.velocity.y < 4.0f) {
                ball_.velocity.y = 11.0f; // keep it bouncing forever for the demo
            }
            ++bounces_;
        }
        const float maxX = WorldWidth - ball_.size.x;
        if (ball_.position.x < 0.0f || ball_.position.x > maxX) {
            ball_.position.x = Math::clamp(ball_.position.x, 0.0f, maxX);
            ball_.velocity.x = -ball_.velocity.x;
        }
    }

    void simulatePlayer(float dt) {
        player_.previous = player_.position;
        const Input& in = input();
        float move = 0.0f;
        if (in.isKeyDown(Key::A) || in.isKeyDown(Key::Left)) {
            move -= 1.0f;
        }
        if (in.isKeyDown(Key::D) || in.isKeyDown(Key::Right)) {
            move += 1.0f;
        }
        player_.velocity.x = move * 6.0f; // 6 m/s

        const bool onGround = player_.position.y <= GroundHeight + 0.001f;
        if (jumpQueued_ && onGround) {
            player_.velocity.y = 9.0f;
        }
        jumpQueued_ = false; // a press that comes while in the air is dropped, not saved for later
        player_.velocity.y += Gravity * dt;
        player_.position += player_.velocity * dt;
        if (player_.position.y < GroundHeight) {
            player_.position.y = GroundHeight;
            player_.velocity.y = 0.0f;
        }
        player_.position.x = Math::clamp(player_.position.x, 0.0f, WorldWidth - player_.size.x);
    }

    // ---------------------------------------------------------------- input
    void handleKeys() {
        const Input& in = input();
        // Edges ("pressed this frame") are read here, once per frame, never in
        // onFixedUpdate: that may run zero times in a frame (and miss the press)
        // or twice (and see it twice). We remember the request instead, and the
        // next fixed step uses it.
        if (in.wasKeyPressed(Key::Space) || in.wasKeyPressed(Key::W) || in.wasKeyPressed(Key::Up)) {
            jumpQueued_ = true;
        }
        if (in.wasKeyPressed(Key::Escape)) {
            quit();
        }
        if (in.wasKeyPressed(Key::I)) {
            interpolate_ = !interpolate_;
            ENGINE_INFO("Interpolation ", interpolate_ ? "on" : "off");
        }
        if (in.wasKeyPressed(Key::V)) {
            window().setVSync(!window().vsync());
            ENGINE_INFO("Vsync ", window().vsync() ? "on" : "off");
        }
        const Key rateKeys[] = {Key::Num1, Key::Num2, Key::Num3, Key::Num4};
        const double rates[] = {10.0, 30.0, 60.0, 120.0};
        for (int i = 0; i < 4; ++i) {
            if (in.wasKeyPressed(rateKeys[i])) {
                timestep().setStepSeconds(1.0 / rates[i]);
                ENGINE_INFO("Fixed update rate: ", rates[i], " Hz");
            }
        }
        if (in.wasKeyPressed(Key::F12)) {
            screenshotRequested_ = true;
        }
        if (in.wasKeyPressed(Key::H)) {
            printHelp();
        }
    }

    void handleMouse() {
        const Input& in = input();
        if (in.wasMousePressed(MouseButton::Left)) {
            const Vec2 world = windowToWorld(in.mousePosition());
            ball_.position = ball_.previous = world - ball_.size * 0.5f;
            ENGINE_INFO("Ball moved to (", world.x, ", ", world.y, ") m");
        }
        if (in.scrollDelta() != 0.0f) {
            const float size = Math::clamp(ball_.size.x + in.scrollDelta() * 0.1f, 0.2f, 3.0f);
            ball_.size = Vec2(size);
        }
    }

    Vec2 windowToWorld(Vec2 windowPos) {
        // Mouse coordinates are window units with the origin top-left;
        // the demo world has its origin bottom-left. Flip y and scale.
        const IVec2 win = window().windowSize();
        return Vec2(windowPos.x / static_cast<float>(win.x) * WorldWidth,
                    (1.0f - windowPos.y / static_cast<float>(win.y)) * WorldHeight);
    }

    // ---------------------------------------------------------------- drawing helpers
    Vec3 backgroundColor() const {
        // A slow drift between two dark blues, driven by real (variable) time.
        const float t = 0.5f + 0.5f * std::sin(backgroundPhase_ * Math::TwoPi);
        return Math::lerp(Vec3(0.07f, 0.08f, 0.13f), Vec3(0.10f, 0.13f, 0.22f), t);
    }

    static void clearTo(const Vec3& color) {
        glClearColor(color.x, color.y, color.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    // Fills a rectangle given in world meters (origin bottom-left, like OpenGL's
    // window coordinates, so only scaling is needed: no flip).
    void fillRect(Vec2 minMeters, Vec2 sizeMeters, const Vec3& color) {
        const IVec2 fb = window().framebufferSize();
        const float sx = static_cast<float>(fb.x) / WorldWidth;
        const float sy = static_cast<float>(fb.y) / WorldHeight;
        const int x = static_cast<int>(std::lround(minMeters.x * sx));
        const int y = static_cast<int>(std::lround(minMeters.y * sy));
        const int w = static_cast<int>(std::lround(sizeMeters.x * sx));
        const int h = static_cast<int>(std::lround(sizeMeters.y * sy));
        glScissor(x, y, w, h);
        clearTo(color);
    }

    // Ten ticks under the ground show how many fixed steps the last frame ran.
    void drawTimelineMarkers() {
        const int steps = stats().fixedStepsThisFrame;
        for (int i = 0; i < 10; ++i) {
            const Vec3 color = i < steps ? Vec3(0.45f, 0.95f, 0.55f) : Vec3(0.30f, 0.28f, 0.34f);
            fillRect(Vec2(0.3f + static_cast<float>(i) * 0.35f, 0.15f), Vec2(0.25f, 0.3f), color);
        }
    }

    void maybeTakeScreenshot(const IVec2& fb) {
        // --screenshot saves the very last frame of a --frames run.
        const bool lastFrame = settings().maxFrames > 0 && stats().frame + 1 == settings().maxFrames;
        if (screenshotRequested_ || (lastFrame && !screenshotPath_.empty())) {
            const std::string path = screenshotPath_.empty() ? "screenshot.ppm" : screenshotPath_;
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
        std::ostringstream title;
        title.precision(1);
        title << std::fixed << "Xordor Sandbox  |  " << stats().fps << " FPS  " << stats().frameMs << " ms  |  fixed "
              << 1.0 / timestep().stepSeconds() << " Hz  |  interpolation " << (interpolate_ ? "on" : "off")
              << "  |  vsync " << (window().vsync() ? "on" : "off");
        window().setTitle(title.str());
    }

    static void printHelp() {
        ENGINE_INFO("Controls: A/D or arrows move, Space jumps, 1-4 fixed rate (10/30/60/120 Hz), "
                    "I interpolation, V vsync, click moves the ball, scroll resizes it, F12 screenshot, Esc quits");
    }

    Body ball_;
    Body player_;
    bool jumpQueued_ = false;
    bool interpolate_ = true;
    bool screenshotRequested_ = false;
    std::string screenshotPath_;
    float backgroundPhase_ = 0.0f;
    double titleTimer_ = 0.0;
    int bounces_ = 0;
};

// Tiny command-line parser: --frames N, --fixed-hz N, --no-vsync, --screenshot FILE.
bool parseOptions(int argc, char** argv, Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--frames" && hasValue) {
            options.frames = std::atoll(argv[++i]);
        } else if (arg == "--fixed-hz" && hasValue) {
            options.fixedHz = std::atof(argv[++i]);
        } else if (arg == "--screenshot" && hasValue) {
            options.screenshotPath = argv[++i];
        } else if (arg == "--no-vsync") {
            options.vsync = false;
        } else {
            ENGINE_ERROR("Unknown option ", arg,
                         ". Options: --frames N, --fixed-hz N, --no-vsync, --screenshot FILE");
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
    // Everything that can fail (no display, no OpenGL 4.1 driver) throws from a
    // constructor. Catching here turns that into a readable message and an
    // exit code instead of a crash.
    try {
        ApplicationSettings settings;
        settings.window.title = "Xordor Sandbox";
        settings.window.vsync = options.vsync;
        settings.fixedUpdateHz = options.fixedHz;
        settings.maxFrames = options.frames;
        settings.logFile = "sandbox.log";
        Sandbox app(settings, options.screenshotPath);
        return app.run();
    } catch (const std::exception& error) {
        ENGINE_ERROR("Fatal: ", error.what());
        return 1;
    }
}
