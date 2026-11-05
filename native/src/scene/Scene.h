#pragma once
// Scene: a world of entities, with a parent-child hierarchy on top of the ECS.
//
//   Scene scene;
//   Entity sun    = scene.createEntity("Sun");
//   Entity planet = scene.createEntity("Planet");
//   scene.setParent(planet, sun);              // planet now moves with the sun
//   scene.get<Transform>(planet).position = {2, 0, 0};   // 2 m from the sun
//
// The hierarchy as a tree:
//
//   Sun          world = Sun.local
//   └─ Planet    world = Sun.world * Planet.local
//      └─ Moon   world = Planet.world * Moon.local
//
// Rotate the sun and the planet swings around it, carrying the moon along.
// Every entity made with createEntity has a Name, a Transform and a Hierarchy.

#include "core/Math.h"
#include "scene/Components.h"
#include "scene/Registry.h"

#include <string>
#include <vector>

namespace Engine {

// Settings for the whole scene rather than one entity.
struct SceneEnvironment {
    Vec3 clearColor{0.47f, 0.62f, 0.80f}; // display (sRGB) space, like a color picker
    Vec3 skyAmbient{0.20f, 0.24f, 0.32f};
    Vec3 groundAmbient{0.10f, 0.09f, 0.08f};
};

class Scene {
public:
    SceneEnvironment environment;

    // ---------------------------------------------------------------- entities
    Entity createEntity(const std::string& name = "Entity");
    // Destroys the entity and all its descendants (like deleting a folder).
    void destroyEntity(Entity e);
    bool isAlive(Entity e) const { return registry_.alive(e); }
    std::size_t entityCount() const { return registry_.aliveCount(); }
    // The first entity with this name, or NullEntity. Linear search: fine for
    // setup code, too slow to call for every entity every frame.
    Entity findByName(const std::string& name) const;
    const std::string& nameOf(Entity e) const;

    // Shortcuts to the registry, so scene.add<MeshRenderer>(e) reads naturally.
    template <typename T, typename... Args>
    T& add(Entity e, Args&&... args) { return registry_.add<T>(e, std::forward<Args>(args)...); }
    template <typename T> T& get(Entity e) { return registry_.get<T>(e); }
    template <typename T> const T& get(Entity e) const { return registry_.get<T>(e); }
    template <typename T> T* tryGet(Entity e) { return registry_.tryGet<T>(e); }
    template <typename T> const T* tryGet(Entity e) const { return registry_.tryGet<T>(e); }
    template <typename T> bool has(Entity e) const { return registry_.has<T>(e); }
    template <typename T> void remove(Entity e) { registry_.remove<T>(e); }

    Registry& registry() { return registry_; }
    const Registry& registry() const { return registry_; }

    // ---------------------------------------------------------------- hierarchy
    // Makes `child` a child of `parent` (NullEntity: make it top-level).
    // keepWorldTransform = true: the child stays where it is on screen, and its
    // local transform is recalculated relative to the new parent.
    // false: it keeps its local numbers, so it jumps to the same offset from
    // the new parent. Returns false (and changes nothing) if it would make a
    // loop, such as parenting an entity to its own grandchild.
    bool setParent(Entity child, Entity parent, bool keepWorldTransform = true);
    Entity parentOf(Entity e) const;
    const std::vector<Entity>& childrenOf(Entity e) const;
    bool isDescendantOf(Entity e, Entity ancestor) const;
    // Top-level entities, in creation order (handy for printing the tree).
    std::vector<Entity> roots() const;

    // ---------------------------------------------------------------- transforms
    // World matrix = parent's world matrix * own local matrix, all the way up.
    // Computed on demand, so it's never out of date.
    // TODO(performance): cache world matrices with "dirty" flags so an entity
    // deep in a big tree isn't recomputed for every query.
    Mat4 worldMatrix(Entity e) const;
    // The same, but every transform on the way up blended by alpha (rendering).
    Mat4 interpolatedWorldMatrix(Entity e, float alpha) const;
    Vec3 worldPosition(Entity e) const { return Vec3(worldMatrix(e)[3]); }

    // Call at the start of every fixed step, before gameplay moves anything:
    // remembers each transform so rendering can blend toward the new one.
    void beginFixedStep();

    // ---------------------------------------------------------------- queries
    // The first entity with a CameraComponent marked primary, or NullEntity.
    Entity primaryCamera() const;

private:
    void detachFromParent(Entity child);

    Registry registry_;
};

} // namespace Engine
