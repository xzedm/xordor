#pragma once
// Application: the main loop. A game or tool subclasses it and overrides the
// on...() functions it needs; run() calls them in this order every frame:
//
//   pollEvents()                       OS events -> Input
//   onFixedUpdate(dt)  x 0..N          gameplay and physics, always with the same dt
//   onUpdate(frameTime)                things that should follow real time (cameras, UI)
//   onRender(alpha)                    drawing; alpha = progress into the next fixed step
//   swapBuffers(), Input::endFrame()
//
// See ARCHITECTURE.md ("How one frame flows") for why there are two update rates.

#include "core/Input.h"
#include "core/Time.h"
#include "core/Window.h"

#include <memory>
#include <string>

namespace Engine {

struct ApplicationSettings {
    WindowSettings window;
    double fixedUpdateHz = 60.0;
    // Stop on its own after this many frames (0 = run until the window closes).
    // Used by automated tests and CI to run an app without anyone watching.
    long long maxFrames = 0;
    std::string logFile; // also log to this file when not empty
};

struct FrameStats {
    long long frame = 0;           // frames finished so far
    double seconds = 0.0;          // real time since run() started
    double frameMs = 0.0;          // duration of the last frame
    double fps = 0.0;              // averaged over about half a second
    int fixedStepsThisFrame = 0;   // 0, 1, or more: depends on frame time vs fixed step
};

class Application {
public:
    explicit Application(ApplicationSettings settings);
    virtual ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // Runs until quit() is called or the window is closed. Returns the exit code.
    int run();

protected:
    // Override what you need. The defaults do nothing on purpose: an app that
    // has nothing to simulate simply doesn't override onFixedUpdate.
    virtual void onStart() {}
    virtual void onFixedUpdate(double dt) { (void)dt; }
    virtual void onUpdate(double frameSeconds) { (void)frameSeconds; }
    virtual void onRender(double alpha) { (void)alpha; }
    virtual void onShutdown() {}
    // Default: makes OpenGL draw to the whole new framebuffer. (The renderer
    // takes this over in Phase 2.)
    virtual void onFramebufferResize(int width, int height);

    Window& window() { return *window_; }
    Input& input() { return input_; }
    FixedTimestep& timestep() { return timestep_; }
    const FrameStats& stats() const { return stats_; }
    const ApplicationSettings& settings() const { return settings_; }

    // Ends the loop after the current frame.
    void quit() { quitRequested_ = true; }

private:
    void connectWindowEvents();
    void updateStats(double frameSeconds, int fixedSteps);

    ApplicationSettings settings_;
    // A pointer (not a plain member) so the window is created in the
    // constructor body, after the log file is set up, and so Window's
    // non-movable type doesn't make Application hard to construct.
    std::unique_ptr<Window> window_;
    Input input_;
    FixedTimestep timestep_;
    Clock clock_;
    FrameStats stats_;
    bool quitRequested_ = false;

    // For the FPS average.
    double fpsTimer_ = 0.0;
    int fpsFrames_ = 0;
};

} // namespace Engine
