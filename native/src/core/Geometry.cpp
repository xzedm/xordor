#include "core/Geometry.h"

#include <algorithm>
#include <cmath>
#include <limits>

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

Ray rayFromScreen(const Vec2& ndc, const Mat4& inverseViewProjection) {
    // Clip-space depth -1 is the near plane and +1 the far plane (OpenGL).
    // After un-projecting, divide by w to get back to ordinary 3D points.
    const Vec4 nearPoint = inverseViewProjection * Vec4(ndc, -1.0f, 1.0f);
    const Vec4 farPoint = inverseViewProjection * Vec4(ndc, 1.0f, 1.0f);
    const Vec3 a = Vec3(nearPoint) / nearPoint.w;
    const Vec3 b = Vec3(farPoint) / farPoint.w;
    return {a, glm::normalize(b - a)};
}

std::optional<float> intersect(const Ray& ray, const Aabb& box) {
    float tNear = 0.0f; // the ray starts at t = 0
    float tFar = std::numeric_limits<float>::max();
    for (int axis = 0; axis < 3; ++axis) {
        const float o = ray.origin[axis];
        const float d = ray.direction[axis];
        if (std::fabs(d) < 1e-12f) {
            // Parallel to this pair of planes: inside the slab or never.
            if (o < box.min[axis] || o > box.max[axis]) return std::nullopt;
            continue;
        }
        float t0 = (box.min[axis] - o) / d;
        float t1 = (box.max[axis] - o) / d;
        if (t0 > t1) std::swap(t0, t1);
        tNear = std::max(tNear, t0);
        tFar = std::min(tFar, t1);
        if (tNear > tFar) return std::nullopt; // the three ranges don't overlap
    }
    return tNear;
}

std::optional<float> intersect(const Ray& ray, const Sphere& sphere) {
    // |origin + t*dir - center|^2 = r^2 is a quadratic in t (dir is unit length).
    const Vec3 oc = ray.origin - sphere.center;
    const float b = glm::dot(oc, ray.direction);
    const float c = glm::dot(oc, oc) - sphere.radius * sphere.radius;
    const float discriminant = b * b - c;
    if (discriminant < 0.0f) return std::nullopt;
    const float root = std::sqrt(discriminant);
    const float t = -b - root; // the nearer crossing
    if (t >= 0.0f) return t;
    return -b + root >= 0.0f ? std::optional<float>(0.0f) : std::nullopt; // inside, or behind us
}

std::optional<float> intersectPlane(const Ray& ray, const Vec3& planePoint, const Vec3& planeNormal) {
    const float denominator = glm::dot(planeNormal, ray.direction);
    if (std::fabs(denominator) < 1e-6f) return std::nullopt; // edge-on
    const float t = glm::dot(planePoint - ray.origin, planeNormal) / denominator;
    return t >= 0.0f ? std::optional<float>(t) : std::nullopt;
}

std::optional<float> closestOnLine(const Ray& ray, const Vec3& linePoint, const Vec3& lineDirection) {
    // Two lines P(t) = o + t*d and Q(s) = p + s*e are closest where the
    // segment between them is perpendicular to both. Solving the two dot
    // products for s gives the formula below (d, e not necessarily unit).
    const Vec3& d = ray.direction;
    const Vec3& e = lineDirection;
    const Vec3 w = ray.origin - linePoint;
    const float a = glm::dot(d, d), b = glm::dot(d, e), c = glm::dot(e, e);
    const float dw = glm::dot(d, w), ew = glm::dot(e, w);
    const float denominator = a * c - b * b;
    if (std::fabs(denominator) < 1e-9f * a * c) return std::nullopt; // parallel
    return (a * ew - b * dw) / denominator;
}

float signedAngle(const Vec3& a, const Vec3& b, const Vec3& axis) {
    // atan2(sin, cos) with sin from the cross product (its component along
    // the axis carries the sign) and cos from the dot product.
    return std::atan2(glm::dot(glm::cross(a, b), glm::normalize(axis)), glm::dot(a, b));
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
