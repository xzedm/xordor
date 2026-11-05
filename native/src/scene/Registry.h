#pragma once
// Registry: creates entities and stores their components, one pool per type.
//
//   Registry registry;
//   Entity crate = registry.create();
//   registry.add<Transform>(crate).position = {0, 1, 0};
//   registry.add<MeshRenderer>(crate, &cubeMesh);
//
//   // A "system" is just a loop over the entities that have some components:
//   registry.each<Transform, Spin>([&](Entity, Transform& t, Spin& s) {
//       t.rotation = glm::angleAxis(s.speed * dt, Math::WorldUp) * t.rotation;
//   });
//
// Components are plain structs: no base class, no virtual functions. Any
// struct can be a component, and behavior lives in the loops that use them.
// Compare the classic "GameObject" class tree (Enemy : Character : Actor),
// where a flying, burning, collectible thing needs three base classes. Here it
// just has three components.

#include "core/Assert.h"
#include "scene/ComponentPool.h"
#include "scene/Entity.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace Engine {

namespace detail {
// Hands out 0, 1, 2... the first time each component type asks.
std::size_t nextComponentTypeId();
} // namespace detail

// A small number for each component type, used to find its pool in a vector.
// The function-local static is initialized once per type T, the first time it
// runs. (Faster to look up than a map keyed by typeid, and needs no RTTI.)
template <typename T>
std::size_t componentTypeId() {
    static const std::size_t id = detail::nextComponentTypeId();
    return id;
}

class Registry {
public:
    Registry() = default;
    // A registry owns everything in it; copying would duplicate a whole world
    // by accident. Moving is fine.
    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;
    Registry(Registry&&) = default;
    Registry& operator=(Registry&&) = default;

    Entity create();
    // Removes all of the entity's components and frees its slot. Destroying a
    // dead or null entity does nothing (and says so in debug builds).
    void destroy(Entity e);
    bool alive(Entity e) const;
    std::size_t aliveCount() const { return aliveCount_; }

    // Adds a component, constructed from args, and returns it. The entity
    // must be alive and must not already have one (assert).
    template <typename T, typename... Args>
    T& add(Entity e, Args&&... args) {
        ENGINE_ASSERT(alive(e), "add() on a dead entity");
        return pool<T>().add(e, std::forward<Args>(args)...);
    }

    // The component, which must exist (assert). Use tryGet when it may not.
    template <typename T>
    T& get(Entity e) {
        T* component = tryGet<T>(e);
        ENGINE_ASSERT(component, "entity has no component of this type");
        return *component;
    }
    template <typename T>
    const T& get(Entity e) const {
        const T* component = tryGet<T>(e);
        ENGINE_ASSERT(component, "entity has no component of this type");
        return *component;
    }

    template <typename T>
    T* tryGet(Entity e) {
        ComponentPool<T>* p = findPool<T>();
        return p ? p->tryGet(e) : nullptr;
    }
    template <typename T>
    const T* tryGet(Entity e) const {
        const ComponentPool<T>* p = findPool<T>();
        return p ? p->tryGet(e) : nullptr;
    }

    template <typename T>
    bool has(Entity e) const {
        const ComponentPool<T>* p = findPool<T>();
        return p && p->has(e);
    }

    template <typename T>
    void remove(Entity e) {
        if (ComponentPool<T>* p = findPool<T>()) {
            p->remove(e);
        }
    }

    template <typename T>
    std::size_t count() const {
        const ComponentPool<T>* p = findPool<T>();
        return p ? p->size() : 0;
    }

    // Calls fn(entity, First&, Rest&...) for every entity that has all the
    // listed components. It walks First's pool, so put the rarest type first.
    //
    // It iterates over a copy of the entity list, so fn may create and destroy
    // entities, or add and remove components, without breaking the loop.
    // Entities destroyed during the loop are skipped. (References passed to
    // fn are still only valid until fn adds or removes that component type.)
    // TODO(performance): the copy costs one allocation per call.
    template <typename First, typename... Rest, typename Fn>
    void each(Fn&& fn) {
        const ComponentPool<First>* first = findPool<First>();
        if (!first) {
            return;
        }
        const std::vector<Entity> snapshot = first->entities();
        for (Entity e : snapshot) {
            First* component = tryGet<First>(e);
            if (component && (has<Rest>(e) && ...)) {
                fn(e, *component, get<Rest>(e)...);
            }
        }
    }

    // Read-only version, for const code such as the renderer.
    template <typename First, typename... Rest, typename Fn>
    void each(Fn&& fn) const {
        const ComponentPool<First>* first = findPool<First>();
        if (!first) {
            return;
        }
        for (Entity e : first->entities()) {
            if ((has<Rest>(e) && ...)) {
                fn(e, *first->tryGet(e), get<Rest>(e)...);
            }
        }
    }

private:
    struct Slot {
        std::uint32_t generation = 0;
        bool alive = false;
    };

    template <typename T>
    ComponentPool<T>& pool() {
        const std::size_t id = componentTypeId<T>();
        if (id >= pools_.size()) {
            pools_.resize(id + 1);
        }
        if (!pools_[id]) {
            pools_[id] = std::make_unique<ComponentPool<T>>();
        }
        return static_cast<ComponentPool<T>&>(*pools_[id]);
    }

    template <typename T>
    ComponentPool<T>* findPool() {
        const std::size_t id = componentTypeId<T>();
        // static_cast is safe: pools_[id] is only ever created as ComponentPool<T>.
        return id < pools_.size() ? static_cast<ComponentPool<T>*>(pools_[id].get()) : nullptr;
    }
    template <typename T>
    const ComponentPool<T>* findPool() const {
        const std::size_t id = componentTypeId<T>();
        return id < pools_.size() ? static_cast<const ComponentPool<T>*>(pools_[id].get()) : nullptr;
    }

    std::vector<Slot> slots_;
    std::vector<std::uint32_t> freeSlots_; // destroyed slots, ready for reuse
    std::vector<std::unique_ptr<ComponentPoolBase>> pools_; // indexed by componentTypeId
    std::size_t aliveCount_ = 0;
};

} // namespace Engine
