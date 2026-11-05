#pragma once
// ComponentPool<T>: every component of one type, packed in one array.
//
// The data structure is a "sparse set":
//
//   sparse_     indexed by entity index -> position in the dense arrays
//               [ -, 2, -, 0, -, 1 ]          (- = this entity has no T)
//
//   entities_   [ e3, e5, e1 ]                which entity owns each slot
//   components_ [ T,  T,  T  ]                the components, no gaps
//
// * has / get / add / remove are all constant time.
// * Iterating walks components_ front to back with no gaps, which is what
//   the CPU cache likes best. That's the main reason engines store
//   components by type instead of inside each object.
// * Removing moves the last element into the hole ("swap and pop"), so the
//   order of components changes. Nothing may depend on that order.
//
// Rule: a reference returned by get() or add() is valid only until the next
// add() or remove() of the same component type. Adding can grow the vector
// and move every component in memory. Keep Entity handles, not references.

#include "core/Assert.h"
#include "scene/Entity.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace Engine {

// The part of a pool that doesn't depend on T, so the registry can keep
// pools of different types in one list and remove an entity from all of them.
class ComponentPoolBase {
public:
    virtual ~ComponentPoolBase() = default;
    virtual bool has(Entity e) const = 0;
    virtual void remove(Entity e) = 0;
    virtual std::size_t size() const = 0;
    virtual const std::vector<Entity>& entities() const = 0;
};

template <typename T>
class ComponentPool final : public ComponentPoolBase {
public:
    bool has(Entity e) const override {
        // Compare the whole handle, generation included: a stale handle whose
        // slot was reused by a new entity must not see the new entity's data.
        const std::uint32_t slot = slotOf(e);
        return slot != Missing && entities_[slot] == e;
    }

    template <typename... Args>
    T& add(Entity e, Args&&... args) {
        ENGINE_ASSERT(!has(e), "entity already has this component; use get() to change it");
        if (has(e)) {
            // Release builds: replace the old value rather than keep two.
            T& existing = components_[slotOf(e)];
            existing = T{std::forward<Args>(args)...};
            return existing;
        }
        if (e.index >= sparse_.size()) {
            sparse_.resize(static_cast<std::size_t>(e.index) + 1, Missing);
        }
        sparse_[e.index] = static_cast<std::uint32_t>(components_.size());
        entities_.push_back(e);
        // T{...} (braces) so plain structs without constructors work:
        // add<Transform>(e) or add<Name>(e, "Crate").
        components_.push_back(T{std::forward<Args>(args)...});
        return components_.back();
    }

    T* tryGet(Entity e) { return has(e) ? &components_[slotOf(e)] : nullptr; }
    const T* tryGet(Entity e) const { return has(e) ? &components_[slotOf(e)] : nullptr; }

    void remove(Entity e) override {
        if (!has(e)) {
            return;
        }
        const std::uint32_t hole = slotOf(e);
        const std::uint32_t last = static_cast<std::uint32_t>(components_.size() - 1);
        if (hole != last) {
            // Move the last component into the hole and fix its sparse entry.
            components_[hole] = std::move(components_[last]);
            entities_[hole] = entities_[last];
            sparse_[entities_[hole].index] = hole;
        }
        components_.pop_back();
        entities_.pop_back();
        sparse_[e.index] = Missing;
    }

    std::size_t size() const override { return components_.size(); }
    const std::vector<Entity>& entities() const override { return entities_; }

private:
    static constexpr std::uint32_t Missing = 0xFFFFFFFFu;

    std::uint32_t slotOf(Entity e) const {
        return e.index < sparse_.size() ? sparse_[e.index] : Missing;
    }

    // TODO(performance): sparse_ grows to the largest entity index; real
    // engines split it into fixed-size pages so a few high indices stay cheap.
    std::vector<std::uint32_t> sparse_;
    std::vector<Entity> entities_;
    std::vector<T> components_;
};

} // namespace Engine
