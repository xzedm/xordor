#include "scene/Scene.h"

#include "core/Log.h"

#include <algorithm>

namespace Engine {

// ---------------------------------------------------------------- Transform

Mat4 Transform::interpolatedLocalMatrix(float alpha) const {
    if (!hasPrevious) {
        return localMatrix();
    }
    // Position and scale blend in a straight line. Rotations use slerp
    // ("spherical" lerp): blending quaternions component by component would
    // speed up and slow down through the turn.
    return Math::composeTRS(Math::lerp(previousPosition, position, alpha), glm::slerp(previousRotation, rotation, alpha),
                            Math::lerp(previousScale, scale, alpha));
}

void Transform::resetInterpolation() {
    previousPosition = position;
    previousRotation = rotation;
    previousScale = scale;
    hasPrevious = true;
}

// ---------------------------------------------------------------- entities

Entity Scene::createEntity(const std::string& name) {
    const Entity e = registry_.create();
    registry_.add<Name>(e, name);
    registry_.add<Transform>(e);
    registry_.add<Hierarchy>(e);
    return e;
}

void Scene::destroyEntity(Entity e) {
    if (!isAlive(e)) {
        return;
    }
    // Children first. Copy the list: destroying a child removes it from ours.
    if (const Hierarchy* h = registry_.tryGet<Hierarchy>(e)) {
        const std::vector<Entity> children = h->children;
        for (Entity child : children) {
            destroyEntity(child);
        }
    }
    detachFromParent(e);
    registry_.destroy(e);
}

Entity Scene::findByName(const std::string& name) const {
    Entity found;
    registry_.each<Name>([&](Entity e, const Name& n) {
        if (found.isNull() && n.value == name) {
            found = e;
        }
    });
    return found;
}

const std::string& Scene::nameOf(Entity e) const {
    static const std::string unnamed = "(unnamed)";
    const Name* n = registry_.tryGet<Name>(e);
    return n ? n->value : unnamed;
}

// ---------------------------------------------------------------- hierarchy

Entity Scene::parentOf(Entity e) const {
    const Hierarchy* h = registry_.tryGet<Hierarchy>(e);
    return h ? h->parent : NullEntity;
}

const std::vector<Entity>& Scene::childrenOf(Entity e) const {
    static const std::vector<Entity> none;
    const Hierarchy* h = registry_.tryGet<Hierarchy>(e);
    return h ? h->children : none;
}

bool Scene::isDescendantOf(Entity e, Entity ancestor) const {
    for (Entity p = parentOf(e); !p.isNull(); p = parentOf(p)) {
        if (p == ancestor) {
            return true;
        }
    }
    return false;
}

std::vector<Entity> Scene::roots() const {
    std::vector<Entity> result;
    registry_.each<Hierarchy>([&](Entity e, const Hierarchy& h) {
        if (h.parent.isNull()) {
            result.push_back(e);
        }
    });
    // The pool's order changes when entities are removed (swap and pop);
    // sorting by index gives a stable, roughly-creation order for display.
    std::sort(result.begin(), result.end(), [](Entity a, Entity b) { return a.index < b.index; });
    return result;
}

bool Scene::setParent(Entity child, Entity parent, bool keepWorldTransform) {
    if (!isAlive(child) || (!parent.isNull() && !isAlive(parent))) {
        ENGINE_WARN("setParent: the child or the parent no longer exists");
        return false;
    }
    if (child == parent || (!parent.isNull() && isDescendantOf(parent, child))) {
        // Parenting A to its own descendant would make a loop, and
        // worldMatrix() would follow it forever.
        ENGINE_WARN("setParent: ", nameOf(child), " can't be a child of ", nameOf(parent),
                    ", which is already below it in the tree");
        return false;
    }
    if (parentOf(child) == parent) {
        return true; // nothing to do
    }

    // Measure where it is *before* changing anything.
    const Mat4 world = worldMatrix(child);

    detachFromParent(child);
    registry_.get<Hierarchy>(child).parent = parent;
    if (!parent.isNull()) {
        registry_.get<Hierarchy>(parent).children.push_back(child);
    }

    Transform& t = registry_.get<Transform>(child);
    if (keepWorldTransform) {
        // world = parentWorld * local, so local = inverse(parentWorld) * world.
        const Mat4 parentWorld = parent.isNull() ? Mat4(1.0f) : worldMatrix(parent);
        Math::decomposeTRS(glm::inverse(parentWorld) * world, t.position, t.rotation, t.scale);
    }
    // The local numbers jumped (or the parent changed under them): don't
    // blend from the old values, which meant something else.
    t.resetInterpolation();
    return true;
}

void Scene::detachFromParent(Entity child) {
    Hierarchy& h = registry_.get<Hierarchy>(child);
    if (h.parent.isNull()) {
        return;
    }
    if (Hierarchy* ph = registry_.tryGet<Hierarchy>(h.parent)) {
        auto& siblings = ph->children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), child), siblings.end());
    }
    h.parent = NullEntity;
}

// ---------------------------------------------------------------- transforms

Mat4 Scene::worldMatrix(Entity e) const {
    const Transform* t = registry_.tryGet<Transform>(e);
    if (!t) {
        return Mat4(1.0f);
    }
    const Entity parent = parentOf(e);
    // Recursion follows the parent chain to the top. setParent refuses loops,
    // so this always ends.
    return parent.isNull() ? t->localMatrix() : worldMatrix(parent) * t->localMatrix();
}

Mat4 Scene::interpolatedWorldMatrix(Entity e, float alpha) const {
    const Transform* t = registry_.tryGet<Transform>(e);
    if (!t) {
        return Mat4(1.0f);
    }
    const Entity parent = parentOf(e);
    const Mat4 local = t->interpolatedLocalMatrix(alpha);
    return parent.isNull() ? local : interpolatedWorldMatrix(parent, alpha) * local;
}

void Scene::beginFixedStep() {
    registry_.each<Transform>([](Entity, Transform& t) { t.resetInterpolation(); });
}

// ---------------------------------------------------------------- queries

Entity Scene::primaryCamera() const {
    Entity found;
    registry_.each<CameraComponent>([&](Entity e, const CameraComponent& c) {
        if (found.isNull() && c.primary) {
            found = e;
        }
    });
    return found;
}

} // namespace Engine
