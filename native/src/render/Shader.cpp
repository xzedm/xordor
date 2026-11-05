#include "render/Shader.h"

#include "core/Log.h"
#include "core/Paths.h"
#include "render/GLCheck.h"
#include "render/ShaderLog.h"

#include <glm/gtc/type_ptr.hpp>

#include <vector>

namespace Engine {
namespace {

// Reads an info log (compiler or linker messages) into a string.
template <typename GetLength, typename GetLog>
std::string readInfoLog(GLuint object, GetLength getLength, GetLog getLog) {
    GLint length = 0;
    getLength(object, GL_INFO_LOG_LENGTH, &length);
    if (length <= 1) {
        return {};
    }
    std::vector<char> text(static_cast<size_t>(length));
    getLog(object, length, nullptr, text.data());
    return std::string(text.data());
}

std::optional<GLShader> compileStage(GLenum stage, const std::string& source, const std::string& label,
                                     std::string* error) {
    GLShader shader(glCreateShader(stage));
    const char* text = source.c_str();
    glShaderSource(shader.id(), 1, &text, nullptr);
    glCompileShader(shader.id());

    GLint ok = GL_FALSE;
    glGetShaderiv(shader.id(), GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        const std::string log = readInfoLog(shader.id(), glGetShaderiv, glGetShaderInfoLog);
        if (error) {
            *error = label + " failed to compile:\n" + annotateShaderLog(source, log);
        }
        return std::nullopt; // `shader` deletes itself on the way out
    }
    return shader;
}

} // namespace

std::optional<Shader> Shader::fromSource(const std::string& name, const std::string& vertexSource,
                                         const std::string& fragmentSource, std::string* error) {
    auto vertex = compileStage(GL_VERTEX_SHADER, vertexSource, name + " (vertex shader)", error);
    if (!vertex) {
        return std::nullopt;
    }
    auto fragment = compileStage(GL_FRAGMENT_SHADER, fragmentSource, name + " (fragment shader)", error);
    if (!fragment) {
        return std::nullopt;
    }

    GLProgram program(glCreateProgram());
    glAttachShader(program.id(), vertex->id());
    glAttachShader(program.id(), fragment->id());
    glLinkProgram(program.id());
    // The linked program keeps its own copy of the compiled code, so the two
    // stage objects can go once linking is done (their GLShader owners delete
    // them when this function returns). Detaching first lets the driver free them.
    glDetachShader(program.id(), vertex->id());
    glDetachShader(program.id(), fragment->id());

    GLint ok = GL_FALSE;
    glGetProgramiv(program.id(), GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        if (error) {
            *error = name + " failed to link:\n" + readInfoLog(program.id(), glGetProgramiv, glGetProgramInfoLog);
        }
        return std::nullopt;
    }
    XE_GL_CHECK("linking a shader");
    return Shader(name, std::move(program));
}

std::optional<Shader> Shader::fromFiles(const std::string& pathWithoutExtension, std::string* error) {
    std::string vertexSource;
    std::string fragmentSource;
    const std::string vertexPath = pathWithoutExtension + ".vert";
    const std::string fragmentPath = pathWithoutExtension + ".frag";
    if (!readTextFile(vertexPath, vertexSource)) {
        if (error) {
            *error = "Could not read " + vertexPath;
        }
        return std::nullopt;
    }
    if (!readTextFile(fragmentPath, fragmentSource)) {
        if (error) {
            *error = "Could not read " + fragmentPath;
        }
        return std::nullopt;
    }
    // Name the shader after its file ("lit"), for log messages.
    const auto slash = pathWithoutExtension.find_last_of("/\\");
    const std::string name = slash == std::string::npos ? pathWithoutExtension : pathWithoutExtension.substr(slash + 1);
    return fromSource(name, vertexSource, fragmentSource, error);
}

void Shader::use() const {
    glUseProgram(program_.id());
}

GLint Shader::location(const std::string& name) const {
    const auto found = locations_.find(name);
    if (found != locations_.end()) {
        return found->second;
    }
    const GLint loc = glGetUniformLocation(program_.id(), name.c_str());
    if (loc == -1) {
        // Not an error: GLSL compilers remove uniforms the shader never uses
        // (common while editing a shader). But it is also what a typo in the
        // name looks like, so say it once instead of ignoring it.
        ENGINE_WARN("Shader ", name_, " has no active uniform \"", name,
                    "\" (misspelled, or unused and removed by the compiler); setting it does nothing");
    }
    locations_.emplace(name, loc); // cached, so the warning appears once per name
    return loc;
}

// glUniform* ignores location -1, so setting a missing uniform is safe.
// Each overload picks the glUniform variant for its type.
void Shader::set(const std::string& name, int value) const { glUniform1i(location(name), value); }
void Shader::set(const std::string& name, float value) const { glUniform1f(location(name), value); }
void Shader::set(const std::string& name, const Vec2& value) const { glUniform2fv(location(name), 1, glm::value_ptr(value)); }
void Shader::set(const std::string& name, const Vec3& value) const { glUniform3fv(location(name), 1, glm::value_ptr(value)); }
void Shader::set(const std::string& name, const Vec4& value) const { glUniform4fv(location(name), 1, glm::value_ptr(value)); }
// GL_FALSE = "don't transpose": GLM stores matrices column by column, the same as GLSL.
void Shader::set(const std::string& name, const Mat3& value) const {
    glUniformMatrix3fv(location(name), 1, GL_FALSE, glm::value_ptr(value));
}
void Shader::set(const std::string& name, const Mat4& value) const {
    glUniformMatrix4fv(location(name), 1, GL_FALSE, glm::value_ptr(value));
}

} // namespace Engine
