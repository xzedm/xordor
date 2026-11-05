#pragma once
// Collision queries: which colliders overlap right now?
//
// Phase 3 only *detects* overlaps; nothing gets pushed apart yet (that's the
// Phase 6 collision response). Detection alone already covers a lot of
// gameplay: pickups, trigger zones, "did the bullet hit".
//
// Each Collider is turned into a world-space shape using the entity's world
// matrix:
//   * a sphere moves with the entity and grows with its largest scale axis;
//   * a box becomes the axis-aligned box *around* the rotated box. For a box
//     turned 45 degrees that is noticeably bigger than the box itself.
//     TODO(Phase 6 or later): oriented boxes (separating axis test) for an exact fit.

#include "core/Geometry.h"
#include "scene/Scene.h"

#include <utility>
#include <vector>

namespace Engine {

struct WorldCollider {
    ColliderShape shape = ColliderShape::Box;
    Aabb box;      // used when shape == Box
    Sphere sphere; // used when shape == Sphere
    // A box around either shape, handy for drawing it or a quick first test.
    Aabb bounds() const;
};

WorldCollider toWorld(const Collider& collider, const Mat4& world);
bool overlaps(const WorldCollider& a, const WorldCollider& b);

// Every pair of entities whose colliders overlap, each pair once.
// Child colliders count separately from their parents', and a parent and
// child touching each other is reported like any other pair.
// TODO(performance): tests every pair, O(n^2). Fine for a few hundred
// colliders; a grid or sort-and-sweep "broad phase" would scale further.
std::vector<std::pair<Entity, Entity>> findOverlaps(const Scene& scene);

// Every entity whose collider overlaps `e`'s collider (not including e).
std::vector<Entity> overlapping(const Scene& scene, Entity e);

} // namespace Engine
