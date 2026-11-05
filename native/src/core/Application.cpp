#include "core/Application.h"

#include "core/Log.h"

#include <glad/gl.h>

#include <algorithm>

namespace Engine {

namespace {
// Real time handed to onUpdate is capped like the fixed accumulator's input,
// so a frame that took 3 seconds (a breakpoint) doesn't fling the camera.
constexpr double MaxFrameSeconds = 0.25;
} // namespace

Application::Application(ApplicationSettings settings)
    : settings_(std::move(settings)),
      // 1 Hz to 1000 Hz keeps the step meaningful; anything else is a typo.
      timestep_(1.0 / std::clamp(settings_.fixedUpdateHz, 1.0, 1000.0)) {
    if (!settings_.logFile.empty() && !Log::setLogFile(settings_.logFile)) {
        ENGINE_WARN("Could not open log file ", settings_.logFile);
    }
    window_ = std::make_unique<Window>(settings_.window);
    connectWindowEvents();
}

Application::~Application() = default;

void Application::connectWindowEvents() {
    // The window reports raw events; Application decides where they go.
    // Window and Input don't know about each other, which keeps each one
    // simple and lets Input be unit tested without a window.
    WindowEvents events;
    events.onKey = [this](Key key, bool down) { input_.onKey(key, down); };
    events.onMouseButton = [this](MouseButton button, bool down) { input_.onMouseButton(button, down); };
    events.onMouseMove = [this](Vec2 position) { input_.onMouseMove(position); };
    events.onScroll = [this](float amount) { input_.onScroll(amount); };
    events.onFocusChange = [this](bool focused) {
        if (!focused) {
            input_.releaseAll();
        }
    };
    events.onFramebufferResize = [this](int width, int height) { onFramebufferResize(width, height); };
    window_->setEvents(std::move(events));
}

void Application::onFramebufferResize(int width, int height) {
    // A minimized window reports 0x0; OpenGL accepts it but there's nothing to draw.
    glViewport(0, 0, width, height);
}

int Application::run() {
    ENGINE_INFO("Running: fixed update ", settings_.fixedUpdateHz, " Hz, vsync ", window_->vsync() ? "on" : "off");
    const IVec2 fb = window_->framebufferSize();
    onFramebufferResize(fb.x, fb.y);
    onStart();

    // Time spent loading in onStart must not count as one giant first frame.
    clock_.tick();

    while (!quitRequested_ && !window_->shouldClose()) {
        const double frameSeconds = clock_.tick();

        // 1. Events: keyboard, mouse and window changes update Input now.
        window_->pollEvents();

        // 2. Fixed updates: as many as the real time that passed calls for.
        const int steps = timestep_.advance(frameSeconds);
        for (int i = 0; i < steps; ++i) {
            onFixedUpdate(timestep_.stepSeconds());
        }

        // 3. Variable update and 4. render, once per frame.
        onUpdate(std::min(frameSeconds, MaxFrameSeconds));
        onRender(timestep_.alpha());

        // 5. Show the frame, then forget this frame's key presses and mouse motion.
        window_->swapBuffers();
        input_.endFrame();

        updateStats(frameSeconds, steps);
        if (settings_.maxFrames > 0 && stats_.frame >= settings_.maxFrames) {
            quit();
        }
    }

    onShutdown();
    if (timestep_.droppedSteps() > 0) {
        ENGINE_WARN("The simulation fell behind real time and skipped ", timestep_.droppedSteps(), " fixed steps");
    }
    ENGINE_INFO("Stopped after ", stats_.frame, " frames and ", timestep_.totalSteps(), " fixed steps");
    return 0;
}

void Application::updateStats(double frameSeconds, int fixedSteps) {
    stats_.frame += 1;
    stats_.seconds = clock_.elapsedSeconds();
    stats_.frameMs = frameSeconds * 1000.0;
    stats_.fixedStepsThisFrame = fixedSteps;

    // Average over half a second: per-frame FPS jumps around too much to read.
    fpsTimer_ += frameSeconds;
    fpsFrames_ += 1;
    if (fpsTimer_ >= 0.5) {
        stats_.fps = fpsFrames_ / fpsTimer_;
        fpsTimer_ = 0.0;
        fpsFrames_ = 0;
    }
}

} // namespace Engine
