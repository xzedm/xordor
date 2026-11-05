#pragma once
// SceneRenderer: turns a Scene into ForwardRenderer calls.
//
//   camera entity  (CameraComponent + Transform)  ->  CameraView
//   light entity   (LightComponent + Transform)   ->  DirectionalLight
//   every MeshRenderer + Transform                ->  renderer.submit(...)
//                (mesh and texture ids are turned into GPU objects by RenderAssets)
//
// The renderer (Phase 2) knows nothing about entities, and the scene knows
// nothing about OpenGL. This file is the only place the two meet, so either
// side can change without touching the other.

#include "render/ForwardRenderer.h"
#include "render/Mesh.h"
#include "scene/RenderAssets.h"
#include "scene/Scene.h"

#include <unordered_set>

namespace Engine {

// Pure functions, testable without a GPU.
// The view from a camera entity: it looks down its -Z axis. Scale on the
// entity (or a parent) is ignored, so a scaled parent can't stretch the image.
CameraView cameraViewFor(const Scene& scene, Entity camera, float aspect, float alpha = 1.0f);
// The sun from a light entity: it shines along the entity's -Z axis.
DirectionalLight directionalLightFor(const Scene& scene, Entity light, float alpha = 1.0f);

// The world-space box used to click on an entity: its mesh's bounds, or a
// small box at its origin for entities with no mesh (cameras, lights,
// groups). Sets *isIcon in the second case.
Aabb pickBounds(const Scene& scene, RenderAssets& assets, Entity e, bool* isIcon = nullptr);
// The nearest entity whose pick box the ray hits, or NullEntity.
// TODO(precision): test the triangles inside the box, so clicking the empty
// corner of a rotated barrel's box doesn't select it.
Entity pickEntity(const Scene& scene, RenderAssets& assets, const Ray& ray);

class SceneRenderer {
public:
    // Needs an OpenGL context: it makes the meshes used to draw colliders.
    explicit SceneRenderer(ForwardRenderer& renderer);

    // Draws the scene through its primary camera, blending every transform by
    // alpha (pass the alpha from Application::onRender).
    // aspect = framebuffer width / height.
    void render(const Scene& scene, RenderAssets& assets, float aspect, float alpha);
    // The same through any camera, such as the editor's own (which isn't an
    // entity, so it never ends up in the saved scene).
    void renderView(const Scene& scene, RenderAssets& assets, const CameraView& view, float alpha);

    bool showColliders = false;            // draw collider shapes as wireframes
    Vec3 colliderColor{0.25f, 1.0f, 0.35f};
    bool showIcons = false;                // small wire spheres for cameras, lights and empty entities
    Entity selected;                       // drawn with a yellow box (the editor's selection)

private:
    void submitColliders(const Scene& scene);
    void submitIcons(const Scene& scene, RenderAssets& assets);
    void submitSelection(const Scene& scene, RenderAssets& assets);

    ForwardRenderer& renderer_; // not owned; must outlive this object
    Mesh debugCube_;
    Mesh debugSphere_;
    // Missing camera or light is logged once, not every frame.
    bool warnedNoCamera_ = false;
    bool warnedNoLight_ = false;
    std::unordered_set<Entity> warnedMissingMesh_;
};

} // namespace Engine
