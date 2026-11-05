#pragma once
// GLObject: owns one OpenGL object name (a GLuint) and deletes it when done.
//
// OpenGL hands out objects as plain numbers (buffer 3, texture 7...). A plain
// GLuint member would be copied by C++'s default copy, and both copies would
// later delete the same object, or neither would. GLObject is move-only, like
// std::unique_ptr: exactly one owner, deleted exactly once.
//
//   GLObject<deleteBuffer> vbo(createBuffer());
//   auto other = std::move(vbo);   // vbo now owns nothing; `other` owns the buffer
//
// The deleter is a template parameter so each kind of object (buffer, vertex
// array, texture, program, shader) gets its own type and can't be mixed up.

#include <glad/gl.h>

#include <utility>

namespace Engine {

template <void (*Delete)(GLuint)>
class GLObject {
public:
    GLObject() = default;
    explicit GLObject(GLuint id) : id_(id) {}
    ~GLObject() { reset(); }

    GLObject(const GLObject&) = delete;
    GLObject& operator=(const GLObject&) = delete;

    GLObject(GLObject&& other) noexcept : id_(std::exchange(other.id_, 0)) {}
    GLObject& operator=(GLObject&& other) noexcept {
        if (this != &other) {
            reset();
            id_ = std::exchange(other.id_, 0);
        }
        return *this;
    }

    GLuint id() const { return id_; }
    explicit operator bool() const { return id_ != 0; }

    // Deletes the object now (0 is "no object" in OpenGL, never a real name).
    void reset() {
        if (id_ != 0) {
            Delete(id_);
            id_ = 0;
        }
    }

private:
    GLuint id_ = 0;
};

// glDelete* functions take an array; these wrappers delete a single name.
// They are plain functions (not lambdas) so they can be template arguments.
inline void deleteBuffer(GLuint id) { glDeleteBuffers(1, &id); }
inline void deleteVertexArray(GLuint id) { glDeleteVertexArrays(1, &id); }
inline void deleteTexture(GLuint id) { glDeleteTextures(1, &id); }
inline void deleteShader(GLuint id) { glDeleteShader(id); }
inline void deleteProgram(GLuint id) { glDeleteProgram(id); }
// Framebuffers are render targets other than the window (used by the GPU tests
// now, and by the editor viewport in Phase 5).
inline void deleteFramebuffer(GLuint id) { glDeleteFramebuffers(1, &id); }
inline void deleteRenderbuffer(GLuint id) { glDeleteRenderbuffers(1, &id); }

using GLBuffer = GLObject<deleteBuffer>;
using GLVertexArray = GLObject<deleteVertexArray>;
using GLTexture = GLObject<deleteTexture>;
using GLShader = GLObject<deleteShader>;
using GLProgram = GLObject<deleteProgram>;
using GLFramebuffer = GLObject<deleteFramebuffer>;
using GLRenderbuffer = GLObject<deleteRenderbuffer>;

} // namespace Engine
