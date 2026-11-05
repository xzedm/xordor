#include "render/GLCheck.h"

#include "core/Log.h"

#include <glad/gl.h>

namespace Engine {

const char* glErrorName(unsigned int error) {
    switch (error) {
    case GL_NO_ERROR: return "GL_NO_ERROR";
    case GL_INVALID_ENUM: return "GL_INVALID_ENUM (an enum argument is not allowed here)";
    case GL_INVALID_VALUE: return "GL_INVALID_VALUE (a number is out of range)";
    case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION (not allowed in the current state)";
    case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION (the framebuffer is incomplete)";
    case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
    default: return "unknown OpenGL error";
    }
}

bool checkGLErrors(const char* what, const char* file, int line) {
    bool ok = true;
    // Several errors can be queued; read until the flag is clear. The limit
    // stops an endless loop if there is no current context (then glGetError
    // itself can keep failing).
    for (int i = 0; i < 16; ++i) {
        const GLenum error = glGetError();
        if (error == GL_NO_ERROR) {
            break;
        }
        ok = false;
        Log::write(LogLevel::Error, file, line, "OpenGL error after ", what, ": ", glErrorName(error));
    }
    return ok;
}

} // namespace Engine
