#include "render/ForwardRenderer.h"

#include "core/Log.h"
#include "core/Paths.h"
#include "render/GLCheck.h"

#include <glad/gl.h>

#include <stdexcept>
#include <system_error>

namespace Engine {
namespace {

const char* LitShaderPath = "shaders/lit"; // + .vert / .frag

Shader loadLitShader() {
    std::string error;
    auto shader = Shader::fromFiles(assetPath(LitShaderPath), &error);
    if (!shader) {
        throw std::runtime_error(error + "\n(The asset folder is " + assetDirectory() +
                                 "; set XORDOR_ASSETS to use another one.)");
    }
    return std::move(*shader);
}

} // namespace

ForwardRenderer::ForwardRenderer() : lit_(loadLitShader()), white_(Texture::solidColor(255, 255, 255)) {
    shaderTime_ = shaderFilesTime();
    ENGINE_INFO("Forward renderer ready (shaders from ", assetDirectory(), ")");
}

void ForwardRenderer::setViewport(int width, int height) {
    width_ = width;
    height_ = height;
}

void ForwardRenderer::beginFrame(const Camera& camera, const DirectionalLight& light, const Vec3& clearColor) {
    camera_ = camera;
    light_ = light;
    queue_.clear();
    stats_ = {};

    glViewport(0, 0, width_, height_);
    // Depth testing: each pixel remembers how far away its closest surface is,
    // and anything further away is not drawn over it. Without it, whatever is
    // drawn last wins, and the far side of a cube can cover the near side.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    // Back-face culling: skip triangles facing away from the camera (their
    // corners appear clockwise on screen). Our meshes are counter-clockwise
    // from outside, so on closed shapes the hidden half is skipped for free.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);

    // The clear color is written as-is, so it is in display (sRGB) space,
    // like a color picked in a paint program.
    glClearColor(clearColor.x, clearColor.y, clearColor.z, 1.0f);
    // Clear depth too: the depth buffer must start "infinitely far" every frame.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void ForwardRenderer::submit(const Mesh& mesh, const Material& material, const Mat4& model) {
    queue_.push_back({&mesh, material, model});
}

void ForwardRenderer::endFrame() {
    lit_.use();

    // Per-frame uniforms: the same for every object, so set once.
    lit_.set("uViewProjection", camera_.viewProjection());
    lit_.set("uCameraPosition", camera_.position);
    lit_.set("uLightDirection", glm::normalize(light_.direction));
    lit_.set("uLightColor", light_.color * light_.intensity);
    lit_.set("uSkyAmbient", light_.skyAmbient);
    lit_.set("uGroundAmbient", light_.groundAmbient);
    // The sampler reads texture unit 0 (OpenGL 4.1 has no layout(binding) for samplers).
    lit_.set("uAlbedo", 0);

    for (const DrawItem& item : queue_) {
        const Material& m = item.material;
        (m.albedo ? *m.albedo : white_).bind(0);
        lit_.set("uBaseColor", m.baseColor);
        lit_.set("uSpecular", m.specular);
        lit_.set("uShininess", m.shininess);
        lit_.set("uUvScale", m.uvScale);
        lit_.set("uModel", item.model);
        lit_.set("uNormalMatrix", Math::normalMatrix(item.model));
        item.mesh->draw();
        stats_.drawCalls += 1;
        stats_.triangles += item.mesh->triangleCount();
    }

    // Leave wireframe off for anything drawn after us (UI in later phases).
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    XE_GL_CHECK("drawing the frame");
}

bool ForwardRenderer::reloadShaders() {
    std::string error;
    auto shader = Shader::fromFiles(assetPath(LitShaderPath), &error);
    shaderTime_ = shaderFilesTime(); // don't retry a broken file every frame
    if (!shader) {
        ENGINE_ERROR("Shader reload failed, keeping the previous version.\n", error);
        return false;
    }
    lit_ = std::move(*shader); // the old program is deleted here
    ENGINE_INFO("Reloaded shader ", lit_.name());
    return true;
}

void ForwardRenderer::reloadShadersIfChanged(double frameSeconds) {
    watchTimer_ += frameSeconds;
    if (watchTimer_ < 0.5) {
        return;
    }
    watchTimer_ = 0.0;
    if (shaderFilesTime() != shaderTime_) {
        reloadShaders();
    }
}

std::filesystem::file_time_type ForwardRenderer::shaderFilesTime() const {
    // The newer of the two files' modification times. error_code versions
    // never throw: a file being saved can briefly be missing.
    std::error_code ec;
    const auto vert = std::filesystem::last_write_time(assetPath(LitShaderPath) + std::string(".vert"), ec);
    const auto frag = std::filesystem::last_write_time(assetPath(LitShaderPath) + std::string(".frag"), ec);
    return vert > frag ? vert : frag;
}

} // namespace Engine
