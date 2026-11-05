#include "scene/Registry.h"

#include "core/Log.h"

namespace Engine {

std::size_t detail::nextComponentTypeId() {
    // Not thread-safe on purpose: entities and components are created on the
    // main thread only. TODO(threads): make this atomic if that ever changes.
    static std::size_t next = 0;
    return next++;
}

Entity Registry::create() {
    std::uint32_t index;
    if (!freeSlots_.empty()) {
        // Reuse a slot. Its generation was bumped when it was freed, so
        // handles to the entity that used it before won't match.
        index = freeSlots_.back();
        freeSlots_.pop_back();
    } else {
        index = static_cast<std::uint32_t>(slots_.size());
        slots_.push_back({});
    }
    slots_[index].alive = true;
    ++aliveCount_;
    return Entity{index, slots_[index].generation};
}

void Registry::destroy(Entity e) {
    if (!alive(e)) {
        ENGINE_TRACE("destroy() on an entity that is already gone (index ", e.index, ")");
        return;
    }
    for (const auto& pool : pools_) {
        if (pool) {
            pool->remove(e);
        }
    }
    Slot& slot = slots_[e.index];
    slot.alive = false;
    ++slot.generation;
    freeSlots_.push_back(e.index);
    --aliveCount_;
}

bool Registry::alive(Entity e) const {
    return e.index < slots_.size() && slots_[e.index].alive && slots_[e.index].generation == e.generation;
}

} // namespace Engine
