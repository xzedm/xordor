#pragma once
// The built-in components. All are plain data: the systems that use them are
// in Scene.cpp (transforms, hierarchy), SceneRenderer.cpp (drawing) and
// Collision.cpp (overlaps).
//
// Naming: render/ already has a Camera class (a free camera you fly around)
// and a DirectionalLight struct (what the renderer draws with). The scene's
// versions are therefore CameraComponent and LightComponent: they describe a
// camera or light *attached to an entity*, which takes its position and
// direction from the entity's Transform.

#include "core/Math.h"
#include "render/ForwardRenderer.h" // Material
#include "render/Mesh.h"
#include "scene/Entity.h"

#include <string>
#include <vector>

namespace Engine {

// A readable name, shown in logs and (Phase 5) in the editor's outliner.
struct Name {
    std::string value;
};

// Where the entity is, relative to its parent (or to the world if it has none).
//
// The previous* fields hold the values at the start of the current fixed
// step. Scene::beginFixedStep() copies them; rendering blends from previous
// to current by alpha, like the ball in the Phase 1 demo, but now for every
// entity automatically.
struct Transform {
    Vec3 position{0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; // (w, x, y, z): identity, no rotation
    Vec3 scale{1.0f};

    Vec3 previousPosition{0.0f};
    Quat previousRotation{1.0f, 0.0f, 0.0f, 0.0f};
    Vec3 previousScale{1.0f};
    // False until the first fixed step (or resetInterpolation) fills in the
    // previous* values. Until then the entity is drawn at its current values,
    // so something created and placed mid-frame doesn't slide in from 0,0,0.
    bool hasPrevious = false;

    Mat4 localMatrix() const { return Math::composeTRS(position, rotation, scale); }
    // Blended between the previous and current values; alpha 1 = current.
    Mat4 interpolatedLocalMatrix(float alpha) const;
    // Call after moving an object by a jump (spawn, teleport, or every frame
    // for things moved outside the fixed update, like a camera), so it isn't
    // drawn sliding from its old place.
    void resetInterpolation();
};

// Parent and children. Managed by the Scene: read it freely, but change it
// only with Scene::setParent, which keeps both sides (the parent's children
// list and the child's parent) in agreement and refuses loops.
struct Hierarchy {
    Entity parent;                // NullEntity for a top-level entity
    std::vector<Entity> children; // in the order they were attached
};

// Draws a mesh at the entity's transform.
struct MeshRenderer {
    // Not owned: the mesh must outlive the component. TODO(Phase 4): refer to
    // meshes by asset id, so the asset system owns them and scenes can be saved.
    const Mesh* mesh = nullptr;
    Material material;
    bool visible = true;
};

// A camera on an entity. It looks down the entity's -Z axis, with its +Y up.
struct CameraComponent {
    float fovY = Math::toRadians(60.0f); // vertical field of view, radians
    float nearPlane = 0.05f;
    float farPlane = 500.0f;
    // The scene renders through the first camera marked primary.
    bool primary = true;
};

// A directional light (the sun). It shines along the entity's -Z axis; the
// position doesn't matter, only the rotation. The renderer supports one, so
// the scene uses the first one it finds.
struct LightComponent {
    Vec3 color{1.0f, 0.95f, 0.88f}; // linear
    float intensity = 1.0f;
};

enum class ColliderShape { Box, Sphere };

// A simple collision shape around the entity. Phase 3 can ask "what overlaps
// what" (scene/Collision.h); Phase 6 adds pushing objects apart.
struct Collider {
    ColliderShape shape = ColliderShape::Box;
    Vec3 halfExtents{0.5f}; // box: half the size along each local axis
    float radius = 0.5f;    // sphere
    Vec3 center{0.0f};      // offset from the entity's origin, in local space
};

} // namespace Engine
