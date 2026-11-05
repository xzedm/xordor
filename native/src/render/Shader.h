#pragma once
// Shader: a compiled and linked GPU program (vertex shader + fragment shader).
//
// The vertex shader runs once per vertex and decides where it lands on screen.
// The fragment shader runs once per covered pixel and decides its color.
// Both are written in GLSL, compiled by the graphics driver at runtime, then
// linked into one "program".
//
// Compiling can fail (it is code, after all), and a failed reload while
// editing must not crash the engine, so creation returns std::optional and an
// error message instead of throwing.

#include "core/Math.h"
#include "render/GLObject.h"

#include <optional>
#include <string>
#include <unordered_map>

namespace Engine {

class Shader {
public:
    // Compiles and links. On failure returns nothing and writes a readable
    // message (with the failing source lines) to *error.
    static std::optional<Shader> fromSource(const std::string& name, const std::string& vertexSource,
                                            const std::string& fragmentSource, std::string* error = nullptr);
    // Reads "<pathWithoutExtension>.vert" and ".frag", e.g. assetPath("shaders/lit").
    static std::optional<Shader> fromFiles(const std::string& pathWithoutExtension, std::string* error = nullptr);

    // Movable, not copyable: one Shader owns one GPU program.
    Shader(Shader&&) = default;
    Shader& operator=(Shader&&) = default;

    // Makes this the program used by the next draw calls.
    void use() const;

    // Uniforms are the shader's inputs that stay the same for a whole draw
    // call (matrices, light color...). Setting one that the shader doesn't
    // use is not an error: drivers remove unused uniforms, and we skip them.
    // Call use() first: these set values on the program in use.
    void set(const std::string& name, int value) const;
    void set(const std::string& name, float value) const;
    void set(const std::string& name, const Vec2& value) const;
    void set(const std::string& name, const Vec3& value) const;
    void set(const std::string& name, const Vec4& value) const;
    void set(const std::string& name, const Mat3& value) const;
    void set(const std::string& name, const Mat4& value) const;

    const std::string& name() const { return name_; }
    GLuint id() const { return program_.id(); }

private:
    Shader(std::string name, GLProgram program) : name_(std::move(name)), program_(std::move(program)) {}
    GLint location(const std::string& name) const;

    std::string name_;
    GLProgram program_;
    // Looking a uniform up by name means string comparisons inside the driver.
    // We remember each location the first time (-1 = "not in this shader").
    // TODO(performance): renderers often look locations up once at load time
    // into a struct of GLints; the map is simpler and fast enough for now.
    mutable std::unordered_map<std::string, GLint> locations_;
};

} // namespace Engine
