#include "core/Geometry.h"

#include <cmath>

namespace Engine {

bool Aabb::contains(const Vec3& p) const {
    return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y && p.z >= min.z && p.z <= max.z;
}

bool overlaps(const Aabb& a, const Aabb& b) {
    return a.min.x <= b.max.x && b.min.x <= a.max.x && //
           a.min.y <= b.max.y && b.min.y <= a.max.y && //
           a.min.z <= b.max.z && b.min.z <= a.max.z;
}

bool overlaps(const Sphere& a, const Sphere& b) {
    // Compare squared distances: same answer, no square root.
    const Vec3 d = a.center - b.center;
    const float reach = a.radius + b.radius;
    return glm::dot(d, d) <= reach * reach;
}

Vec3 closestPoint(const Aabb& box, const Vec3& p) {
    // Clamping each coordinate into the box's range gives the nearest point.
    return glm::clamp(p, box.min, box.max);
}

bool overlaps(const Sphere& s, const Aabb& box) {
    // The sphere touches the box if the box's nearest point is within reach.
    const Vec3 d = closestPoint(box, s.center) - s.center;
    return glm::dot(d, d) <= s.radius * s.radius;
}

Aabb transformAabb(const Aabb& local, const Mat4& m) {
    // Jim Arvo's method (Graphics Gems, 1990). Each output axis is a sum of
    // contributions from the input axes; the box's extent along it is the sum
    // of the absolute contributions. Cheaper than transforming all 8 corners,
    // and gives the same box.
    const Vec3 center = Math::transformPoint(m, local.center());
    const Vec3 half = local.halfExtents();
    Vec3 extent(0.0f);
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            extent[row] += std::fabs(m[col][row]) * half[col]; // GLM is m[column][row]
        }
    }
    return {center - extent, center + extent};
}

} // namespace Engine
