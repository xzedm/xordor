#pragma once
// Entity: a handle to "a thing in the scene". It holds no data of its own;
// everything it is (position, mesh, light...) lives in components stored by
// the Registry, looked up by this handle.
//
// A handle is two numbers:
//
//   index        which slot in the registry this entity uses
//   generation   how many times that slot has been reused
//
// When an entity is destroyed, its slot's generation goes up and the slot can
// be reused. Old handles still carry the old generation, so the registry can
// tell they're stale ("that crate was destroyed") instead of silently
// pointing at whatever new entity got the slot. This is the same idea as a
// weak pointer, without any memory allocation.

#include <cstdint>
#include <functional>

namespace Engine {

struct Entity {
    static constexpr std::uint32_t InvalidIndex = 0xFFFFFFFFu;

    std::uint32_t index = InvalidIndex;
    std::uint32_t generation = 0;

    // A default-constructed Entity is "no entity", like nullptr.
    bool isNull() const { return index == InvalidIndex; }

    friend bool operator==(Entity a, Entity b) { return a.index == b.index && a.generation == b.generation; }
    friend bool operator!=(Entity a, Entity b) { return !(a == b); }
};

inline constexpr Entity NullEntity{};

} // namespace Engine

// Lets Entity be a key in std::unordered_map / std::unordered_set.
template <>
struct std::hash<Engine::Entity> {
    std::size_t operator()(Engine::Entity e) const noexcept {
        return std::hash<std::uint64_t>()((std::uint64_t(e.generation) << 32) | e.index);
    }
};
