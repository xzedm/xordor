#include "core/Window.h"

#include "core/Log.h"

// glad must come before GLFW: it provides the OpenGL declarations.
#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <stdexcept>

namespace Engine {

// Our key codes reuse GLFW's numbers (see KeyCodes.h). If a GLFW update ever
// changed them, these fail at compile time instead of keys silently mixing up.
static_assert(static_cast<int>(Key::Space) == GLFW_KEY_SPACE);
static_assert(static_cast<int>(Key::Num0) == GLFW_KEY_0);
static_assert(static_cast<int>(Key::A) == GLFW_KEY_A);
static_assert(static_cast<int>(Key::Z) == GLFW_KEY_Z);
static_assert(static_cast<int>(Key::Escape) == GLFW_KEY_ESCAPE);
static_assert(static_cast<int>(Key::End) == GLFW_KEY_END);
static_assert(static_cast<int>(Key::Pause) == GLFW_KEY_PAUSE);
static_assert(static_cast<int>(Key::F12) == GLFW_KEY_F12);
static_assert(static_cast<int>(Key::KeypadAdd) == GLFW_KEY_KP_ADD);
static_assert(static_cast<int>(Key::KeypadSubtract) == GLFW_KEY_KP_SUBTRACT);
static_assert(static_cast<int>(Key::LeftShift) == GLFW_KEY_LEFT_SHIFT);
static_assert(static_cast<int>(Key::Menu) == GLFW_KEY_MENU);
static_assert(KeyCount == GLFW_KEY_LAST + 1);
static_assert(static_cast<int>(MouseButton::Left) == GLFW_MOUSE_BUTTON_LEFT);
static_assert(static_cast<int>(MouseButton::Right) == GLFW_MOUSE_BUTTON_RIGHT);
static_assert(static_cast<int>(MouseButton::Middle) == GLFW_MOUSE_BUTTON_MIDDLE);
static_assert(MouseButtonCount == GLFW_MOUSE_BUTTON_LAST + 1);

// ---------------------------------------------------------------- GLFW library lifetime
//
// glfwInit() must be called once before any window, and glfwTerminate() once
// after the last window is gone. A shared_ptr counts the windows for us: each
// Window holds one, and when the last copy is released the destructor below
// terminates GLFW. A weak_ptr remembers the library without keeping it alive.
class GlfwLibrary {
public:
    GlfwLibrary() {
        glfwSetErrorCallback([](int code, const char* description) {
            ENGINE_ERROR("GLFW error ", code, ": ", description);
        });
        if (glfwInit() != GLFW_TRUE) {
            throw std::runtime_error("Could not initialize GLFW (no display available?)");
        }
        ENGINE_INFO("GLFW ", glfwGetVersionString());
    }
    ~GlfwLibrary() {
        glfwTerminate();
    }
    GlfwLibrary(const GlfwLibrary&) = delete;
    GlfwLibrary& operator=(const GlfwLibrary&) = delete;

    static std::shared_ptr<GlfwLibrary> acquire() {
        static std::weak_ptr<GlfwLibrary> current;
        if (auto existing = current.lock()) {
            return existing;
        }
        auto library = std::make_shared<GlfwLibrary>();
        current = library;
        return library;
    }
};

void Window::GlfwWindowDeleter::operator()(GLFWwindow* window) const {
    glfwDestroyWindow(window);
}

namespace {

const char* glString(GLenum name) {
    // glGetString returns unsigned chars; every OpenGL string is plain ASCII.
    const GLubyte* text = glGetString(name);
    return text ? reinterpret_cast<const char*>(text) : "(unknown)";
}

} // namespace

Window::Window(const WindowSettings& settings) : glfw_(GlfwLibrary::acquire()), vsync_(settings.vsync) {
    glfwDefaultWindowHints();
    // Ask for exactly OpenGL 4.1 core: the newest version macOS supports, and
    // "core" removes the old fixed-function API so we can't use it by accident.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    // macOS refuses core contexts that are not "forward compatible".
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, settings.resizable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, settings.visible ? GLFW_TRUE : GLFW_FALSE);
    // 24-bit depth buffer for 3D in Phase 2; 8 bits of stencil cost nothing extra.
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);

    GLFWwindow* raw = glfwCreateWindow(settings.width, settings.height, settings.title.c_str(), nullptr, nullptr);
    if (!raw) {
        throw std::runtime_error(
            "Could not create a window with OpenGL 4.1. Updating your graphics driver usually fixes this.");
    }
    handle_.reset(raw); // from here on the unique_ptr owns it, even if we throw below

    // A context must be "current" on a thread before any gl* call works there.
    glfwMakeContextCurrent(raw);
    if (gladLoadGL(glfwGetProcAddress) == 0) {
        throw std::runtime_error("Could not load OpenGL functions");
    }
    ENGINE_INFO("OpenGL ", glString(GL_VERSION), " | ", glString(GL_RENDERER), " | GLSL ",
                glString(GL_SHADING_LANGUAGE_VERSION));

    installCallbacks();
    setVSync(settings.vsync);
    const IVec2 fb = framebufferSize();
    ENGINE_INFO("Window \"", settings.title, "\" ", settings.width, "x", settings.height, " (framebuffer ", fb.x, "x",
                fb.y, ")");
}

Window::~Window() = default; // handle_ closes the window, then glfw_ may terminate GLFW

Window& Window::fromHandle(GLFWwindow* handle) {
    return *static_cast<Window*>(glfwGetWindowUserPointer(handle));
}

void Window::installCallbacks() {
    GLFWwindow* w = handle_.get();
    // GLFW's callbacks are plain C function pointers and can't capture "this".
    // The user pointer is GLFW's way to get back to our object from inside them.
    glfwSetWindowUserPointer(w, this);

    // Lambdas without captures convert to the function pointers GLFW expects.
    glfwSetKeyCallback(w, [](GLFWwindow* handle, int key, int /*scancode*/, int action, int /*mods*/) {
        // GLFW_REPEAT is the OS auto-repeat while a key is held (for typing).
        // Input already knows the key is down, so repeats are ignored.
        if (action == GLFW_REPEAT) {
            return;
        }
        auto& self = fromHandle(handle);
        if (self.events_.onKey) {
            self.events_.onKey(static_cast<Key>(key), action == GLFW_PRESS);
        }
    });
    glfwSetMouseButtonCallback(w, [](GLFWwindow* handle, int button, int action, int /*mods*/) {
        auto& self = fromHandle(handle);
        if (self.events_.onMouseButton) {
            self.events_.onMouseButton(static_cast<MouseButton>(button), action == GLFW_PRESS);
        }
    });
    glfwSetCursorPosCallback(w, [](GLFWwindow* handle, double x, double y) {
        auto& self = fromHandle(handle);
        if (self.events_.onMouseMove) {
            self.events_.onMouseMove(Vec2(static_cast<float>(x), static_cast<float>(y)));
        }
    });
    glfwSetScrollCallback(w, [](GLFWwindow* handle, double /*x*/, double y) {
        auto& self = fromHandle(handle);
        if (self.events_.onScroll) {
            self.events_.onScroll(static_cast<float>(y));
        }
    });
    glfwSetFramebufferSizeCallback(w, [](GLFWwindow* handle, int width, int height) {
        auto& self = fromHandle(handle);
        if (self.events_.onFramebufferResize) {
            self.events_.onFramebufferResize(width, height);
        }
    });
    glfwSetWindowFocusCallback(w, [](GLFWwindow* handle, int focused) {
        auto& self = fromHandle(handle);
        if (self.events_.onFocusChange) {
            self.events_.onFocusChange(focused == GLFW_TRUE);
        }
    });
}

void Window::setEvents(WindowEvents events) {
    events_ = std::move(events);
}

void Window::pollEvents() {
    glfwPollEvents();
}

void Window::swapBuffers() {
    glfwSwapBuffers(handle_.get());
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(handle_.get()) == GLFW_TRUE;
}

void Window::requestClose() {
    glfwSetWindowShouldClose(handle_.get(), GLFW_TRUE);
}

void Window::setTitle(const std::string& title) {
    glfwSetWindowTitle(handle_.get(), title.c_str());
}

void Window::setVSync(bool enabled) {
    // The swap interval is how many monitor refreshes to wait per swap:
    // 1 = sync to the monitor, 0 = present immediately (may tear).
    // It applies to the current context, which is ours (single window, one thread).
    glfwSwapInterval(enabled ? 1 : 0);
    vsync_ = enabled;
}

void Window::setCursorCaptured(bool captured) {
    GLFWwindow* w = handle_.get();
    glfwSetInputMode(w, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    // Raw motion skips the OS pointer acceleration: better for camera control.
    if (glfwRawMouseMotionSupported() == GLFW_TRUE) {
        glfwSetInputMode(w, GLFW_RAW_MOUSE_MOTION, captured ? GLFW_TRUE : GLFW_FALSE);
    }
    cursorCaptured_ = captured;
}

IVec2 Window::framebufferSize() const {
    IVec2 size{0};
    glfwGetFramebufferSize(handle_.get(), &size.x, &size.y);
    return size;
}

IVec2 Window::windowSize() const {
    IVec2 size{0};
    glfwGetWindowSize(handle_.get(), &size.x, &size.y);
    return size;
}

} // namespace Engine
