#pragma once
// SceneRenderer: turns a Scene into ForwardRenderer calls.
//
//   camera entity  (CameraComponent + Transform)  ->  CameraView
//   light entity   (LightComponent + Transform)   ->  DirectionalLight
//   every MeshRenderer + Transform                ->  renderer.submit(...)
//
// The renderer (Phase 2) knows nothing about entities, and the scene knows
// nothing about OpenGL. This file is the only place the two meet, so either
// side can change without touching the other.

#include "render/ForwardRenderer.h"
#include "render/Mesh.h"
#include "scene/Scene.h"

namespace Engine {

// Pure functions, testable without a GPU.
// The view from a camera entity: it looks down its -Z axis. Scale on the
// entity (or a parent) is ignored, so a scaled parent can't stretch the image.
CameraView cameraViewFor(const Scene& scene, Entity camera, float aspect, float alpha = 1.0f);
// The sun from a light entity: it shines along the entity's -Z axis.
DirectionalLight directionalLightFor(const Scene& scene, Entity light, float alpha = 1.0f);

class SceneRenderer {
public:
    // Needs an OpenGL context: it makes the meshes used to draw colliders.
    explicit SceneRenderer(ForwardRenderer& renderer);

    // Draws the scene through its primary camera, blending every transform by
    // alpha (pass the alpha from Application::onRender).
    // aspect = framebuffer width / height.
    void render(const Scene& scene, float aspect, float alpha);

    bool showColliders = false;            // draw collider shapes as wireframes
    Vec3 colliderColor{0.25f, 1.0f, 0.35f};

private:
    void submitColliders(const Scene& scene);

    ForwardRenderer& renderer_; // not owned; must outlive this object
    Mesh debugCube_;
    Mesh debugSphere_;
    // Missing camera or light is logged once, not every frame.
    bool warnedNoCamera_ = false;
    bool warnedNoLight_ = false;
};

} // namespace Engine
