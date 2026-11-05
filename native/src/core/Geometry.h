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

#include <optional>

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

// A half-line: every point origin + t * direction with t >= 0. The editor
// shoots one from the camera through the mouse to find what's under it.
struct Ray {
    Vec3 origin{0.0f};
    Vec3 direction{0.0f, 0.0f, -1.0f}; // unit length
    Vec3 at(float t) const { return origin + direction * t; }
};

// The ray through a point on screen. ndc is the point in normalized device
// coordinates (-1..1, +y up); inverseViewProjection undoes the camera. It
// un-projects the point at the near and far planes and joins them.
Ray rayFromScreen(const Vec2& ndc, const Mat4& inverseViewProjection);

// Distance along the ray to where it enters the box, or nothing if it misses.
// (0 if the origin is inside.) The "slab" method: intersect the ray with the
// three pairs of parallel planes and keep the overlap of the three ranges.
std::optional<float> intersect(const Ray& ray, const Aabb& box);
std::optional<float> intersect(const Ray& ray, const Sphere& sphere);
// Where the ray crosses a plane (point + normal), if in front of it.
std::optional<float> intersectPlane(const Ray& ray, const Vec3& planePoint, const Vec3& planeNormal);

// The point on the infinite line (linePoint + s * lineDirection) closest to the
// ray, as the parameter s. Gizmos drag along an axis with this: the mouse ray
// and the axis rarely meet, so we take the closest approach. Nothing if the
// ray runs parallel to the line (no single closest point).
std::optional<float> closestOnLine(const Ray& ray, const Vec3& linePoint, const Vec3& lineDirection);

// The angle from a to b around `axis`, in radians, -pi..pi, positive
// counter-clockwise when looking down the axis toward its origin
// (right-hand rule). a and b should be perpendicular to the axis.
float signedAngle(const Vec3& a, const Vec3& b, const Vec3& axis);

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
