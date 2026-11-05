#pragma once
// Simple shapes and overlap tests: axis-aligned boxes (AABBs) and spheres.
// Pure math, no scene or GPU, so they're easy to test and reuse (collision now,
// view-frustum culling and mouse picking later).
//
// "Axis-aligned" means the box's edges run along world X, Y and Z: it can't
// be rotated. That makes the overlap test three simple range checks:
//
//      a.min.x        a.max.x
//         [=============]
//                 [==========]        overlap on x if a.min.x <= b.max.x
//              b.min.x     b.max.x                 and b.min.x <= a.max.x
//
// Two boxes overlap only if their ranges overlap on all three axes.

#include "core/Math.h"

namespace Engine {

struct Aabb {
    Vec3 min{0.0f};
    Vec3 max{0.0f};

    static Aabb fromCenter(const Vec3& center, const Vec3& halfExtents) {
        return {center - halfExtents, center + halfExtents};
    }
    Vec3 center() const { return (min + max) * 0.5f; }
    Vec3 halfExtents() const { return (max - min) * 0.5f; }
    bool contains(const Vec3& p) const;
};

struct Sphere {
    Vec3 center{0.0f};
    float radius = 0.0f;
};

// Touching counts as overlapping in all three tests.
bool overlaps(const Aabb& a, const Aabb& b);
bool overlaps(const Sphere& a, const Sphere& b);
bool overlaps(const Sphere& s, const Aabb& box);
inline bool overlaps(const Aabb& box, const Sphere& s) { return overlaps(s, box); }

// The point of the box nearest to p (p itself if it's inside).
Vec3 closestPoint(const Aabb& box, const Vec3& p);

// The smallest axis-aligned box around `local` after transforming it by m.
// A rotated box sticks out further along the axes, so the result is bigger
// than the box itself; it's a conservative fit, not an exact one.
Aabb transformAabb(const Aabb& local, const Mat4& m);

} // namespace Engine
