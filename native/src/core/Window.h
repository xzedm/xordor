#pragma once
// Window: owns an OS window with an OpenGL 4.1 core context.
//
// Ownership (RAII): creating a Window opens the window and the context;
// destroying it closes them. There is no init()/shutdown() pair to forget.
// If creation fails, the constructor throws, so a Window object that exists
// is always usable.
//
// GLFW is an implementation detail. This header only forward-declares its
// type, so code that uses Window never sees GLFW, and swapping GLFW for SDL
// later would only touch Window.cpp.

#include "core/KeyCodes.h"
#include "core/Math.h"

#include <functional>
#include <memory>
#include <string>

struct GLFWwindow;

namespace Engine {

class GlfwLibrary; // keeps GLFW initialized while any window exists (Window.cpp)

struct WindowSettings {
    std::string title = "Xordor";
    int width = 1280;  // in screen coordinates (see framebufferSize below)
    int height = 720;
    bool vsync = true; // wait for the monitor's refresh: no tearing, no wasted frames
    bool resizable = true;
    bool visible = true;
};

// Functions the window calls while pollEvents() runs. Any of them may be empty.
struct WindowEvents {
    std::function<void(Key key, bool down)> onKey;
    std::function<void(MouseButton button, bool down)> onMouseButton;
    std::function<void(Vec2 position)> onMouseMove; // window coordinates, origin top-left
    std::function<void(float amount)> onScroll;
    std::function<void(int width, int height)> onFramebufferResize;
    std::function<void(bool focused)> onFocusChange;
};

class Window {
public:
    explicit Window(const WindowSettings& settings);
    ~Window();

    // GLFW keeps a pointer to this object (to find it from its callbacks), so
    // the object must never move or be copied.
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    void setEvents(WindowEvents events);

    // Handles pending OS events, calling the WindowEvents functions.
    void pollEvents();
    // Shows the frame that was just drawn (double buffering: we draw into a
    // hidden back buffer, then swap it to the front all at once).
    void swapBuffers();

    bool shouldClose() const;
    void requestClose();

    void setTitle(const std::string& title);
    void setVSync(bool enabled);
    bool vsync() const { return vsync_; }

    // Hides the cursor and keeps it in the window, for mouse-look cameras.
    void setCursorCaptured(bool captured);
    bool cursorCaptured() const { return cursorCaptured_; }

    // On high-DPI screens the two differ: a 1280x720 window can have a
    // 2560x1440 framebuffer. OpenGL draws in framebuffer pixels; the mouse
    // reports window coordinates.
    IVec2 framebufferSize() const;
    IVec2 windowSize() const;

private:
    struct GlfwWindowDeleter {
        void operator()(GLFWwindow* window) const;
    };

    void installCallbacks();
    static Window& fromHandle(GLFWwindow* handle);

    // Members are destroyed in reverse order: the window is closed first, and
    // only then may GLFW shut down. So glfw_ must be declared before handle_.
    std::shared_ptr<GlfwLibrary> glfw_;
    std::unique_ptr<GLFWwindow, GlfwWindowDeleter> handle_;
    WindowEvents events_;
    bool vsync_ = true;
    bool cursorCaptured_ = false;
};

} // namespace Engine
