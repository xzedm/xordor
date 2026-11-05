#pragma once
// ForwardRenderer: draws meshes lit by one directional light.
//
// "Forward" means each object is drawn once, computing its final lit color
// directly in the fragment shader. (The alternative, "deferred", first writes
// surface data for the whole screen and lights it afterwards; it suits scenes
// with hundreds of lights, which we don't have.)
//
// Use it like this, every frame:
//
//   renderer.beginFrame(camera, sun, clearColor);
//   renderer.submit(cubeMesh, crateMaterial, modelMatrix);   // any number of times
//   renderer.endFrame();                                     // draws everything
//
// submit() only records what to draw. Collecting first and drawing at the end
// leaves room to sort by shader or texture, or skip what the camera can't see.
// TODO(performance): sort the queue by texture and mesh, and add frustum culling.

#include "core/Math.h"
#include "render/Camera.h"
#include "render/Mesh.h"
#include "render/Shader.h"
#include "render/Texture.h"

#include <filesystem>
#include <vector>

namespace Engine {

struct DirectionalLight {
    // The direction the light travels, from the sun toward the scene (normalized when used).
    Vec3 direction = glm::normalize(Vec3(-0.4f, -1.0f, -0.3f));
    Vec3 color{1.0f, 0.95f, 0.88f}; // linear
    float intensity = 1.0f;
    Vec3 skyAmbient{0.20f, 0.24f, 0.32f};
    Vec3 groundAmbient{0.10f, 0.09f, 0.08f};
};

struct Material {
    Vec3 baseColor{1.0f};             // linear; multiplied with the texture
    const Texture* albedo = nullptr;  // not owned; nullptr means plain white
    float specular = 0.2f;
    float shininess = 32.0f;
    Vec2 uvScale{1.0f};
};

struct RenderStats {
    int drawCalls = 0;
    size_t triangles = 0;
};

class ForwardRenderer {
public:
    // Loads the shaders from the assets folder. Throws std::runtime_error if
    // they are missing or don't compile: nothing can be drawn without them.
    ForwardRenderer();

    void setViewport(int width, int height);

    void beginFrame(const Camera& camera, const DirectionalLight& light, const Vec3& clearColor);
    void submit(const Mesh& mesh, const Material& material, const Mat4& model);
    void endFrame();

    // Recompiles the shaders from disk. On a compile error the old shaders stay
    // in use and the error is logged, so a typo never stops the program.
    bool reloadShaders();
    // Reloads automatically when a shader file was saved (checks twice a second).
    void reloadShadersIfChanged(double frameSeconds);

    bool wireframe = false;
    const RenderStats& stats() const { return stats_; }

private:
    struct DrawItem {
        const Mesh* mesh;
        Material material;
        Mat4 model;
    };

    std::filesystem::file_time_type shaderFilesTime() const;

    Shader lit_;
    Texture white_;
    std::vector<DrawItem> queue_;
    Camera camera_;
    DirectionalLight light_;
    RenderStats stats_;
    int width_ = 1;
    int height_ = 1;
    std::filesystem::file_time_type shaderTime_{};
    double watchTimer_ = 0.0;
};

} // namespace Engine
