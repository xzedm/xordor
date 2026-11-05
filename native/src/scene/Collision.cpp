#include "scene/Collision.h"

#include <algorithm>

namespace Engine {

Aabb WorldCollider::bounds() const {
    if (shape == ColliderShape::Sphere) {
        return Aabb::fromCenter(sphere.center, Vec3(sphere.radius));
    }
    return box;
}

WorldCollider toWorld(const Collider& collider, const Mat4& world) {
    WorldCollider result;
    result.shape = collider.shape;
    if (collider.shape == ColliderShape::Sphere) {
        result.sphere.center = Math::transformPoint(world, collider.center);
        // A sphere scaled unevenly would be an ellipsoid, which this test can't
        // do; using the largest axis keeps it at least as big as the real shape.
        const float scale = std::max({glm::length(Vec3(world[0])), glm::length(Vec3(world[1])),
                                      glm::length(Vec3(world[2]))});
        result.sphere.radius = collider.radius * scale;
    } else {
        result.box = transformAabb(Aabb::fromCenter(collider.center, collider.halfExtents), world);
    }
    return result;
}

bool overlaps(const WorldCollider& a, const WorldCollider& b) {
    const bool aSphere = a.shape == ColliderShape::Sphere;
    const bool bSphere = b.shape == ColliderShape::Sphere;
    if (aSphere && bSphere) {
        return overlaps(a.sphere, b.sphere);
    }
    if (aSphere) {
        return overlaps(a.sphere, b.box);
    }
    if (bSphere) {
        return overlaps(b.sphere, a.box);
    }
    return overlaps(a.box, b.box);
}

namespace {

struct Entry {
    Entity entity;
    WorldCollider shape;
};

std::vector<Entry> worldColliders(const Scene& scene) {
    std::vector<Entry> entries;
    scene.registry().each<Collider>([&](Entity e, const Collider& c) {
        entries.push_back({e, toWorld(c, scene.worldMatrix(e))});
    });
    return entries;
}

} // namespace

std::vector<std::pair<Entity, Entity>> findOverlaps(const Scene& scene) {
    const std::vector<Entry> entries = worldColliders(scene);
    std::vector<std::pair<Entity, Entity>> pairs;
    for (size_t i = 0; i < entries.size(); ++i) {
        for (size_t j = i + 1; j < entries.size(); ++j) { // j > i: each pair once, never with itself
            if (overlaps(entries[i].shape, entries[j].shape)) {
                pairs.emplace_back(entries[i].entity, entries[j].entity);
            }
        }
    }
    return pairs;
}

std::vector<Entity> overlapping(const Scene& scene, Entity e) {
    std::vector<Entity> result;
    const Collider* own = scene.tryGet<Collider>(e);
    if (!own) {
        return result;
    }
    const WorldCollider shape = toWorld(*own, scene.worldMatrix(e));
    scene.registry().each<Collider>([&](Entity other, const Collider& c) {
        if (other != e && overlaps(shape, toWorld(c, scene.worldMatrix(other)))) {
            result.push_back(other);
        }
    });
    return result;
}

} // namespace Engine
