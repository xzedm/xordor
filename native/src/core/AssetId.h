#pragma once
// AssetId: a 128-bit random id (a GUID) that names one asset forever.
//
// Scenes refer to meshes and textures by id, never by file path. The id of a
// file is stored next to it in a small ".meta" file (assets/AssetRegistry.h),
// so a file can be renamed or moved to another folder, as long as its .meta
// goes with it, and every scene that uses it keeps working.
//
// Why 128 random bits? Ids are made on different machines with no central
// counter, by different people adding files. With 2^122 possible values (a
// version-4 GUID keeps 6 bits for its format), two ever colliding is so
// unlikely it can be ignored.
//
// Text form: 8-4-4-4-12 hex digits, e.g. "3f2b8c1e-9d4a-4f6b-8e21-c0ffee000001".
//
// This lives in core/ rather than assets/ because scene components hold ids
// too, and scene/ comes before assets/ in the module order.

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace Engine {

struct AssetId {
    std::uint64_t high = 0;
    std::uint64_t low = 0;

    // All zeros means "no asset", like a null pointer.
    bool isNull() const { return high == 0 && low == 0; }

    // A new random id (version 4 GUID).
    static AssetId generate();
    // Parses the 8-4-4-4-12 form (either case). Nothing if it's malformed.
    static std::optional<AssetId> parse(const std::string& text);
    std::string toString() const; // lowercase 8-4-4-4-12

    friend bool operator==(const AssetId& a, const AssetId& b) { return a.high == b.high && a.low == b.low; }
    friend bool operator!=(const AssetId& a, const AssetId& b) { return !(a == b); }
    friend bool operator<(const AssetId& a, const AssetId& b) {
        return a.high != b.high ? a.high < b.high : a.low < b.low;
    }
};

// Fixed ids for the meshes the engine makes in code. They aren't files, but
// scenes refer to them the same way. The small numbers can't clash with
// generated ids, whose version bits are never zero.
namespace BuiltinAssets {
inline constexpr AssetId Cube{0, 1};
inline constexpr AssetId Plane{0, 2};
inline constexpr AssetId Sphere{0, 3};
} // namespace BuiltinAssets

} // namespace Engine

template <>
struct std::hash<Engine::AssetId> {
    std::size_t operator()(const Engine::AssetId& id) const noexcept {
        // The bits are random already, so mixing the halves is enough.
        return std::hash<std::uint64_t>()(id.high ^ (id.low * 0x9E3779B97F4A7C15ull));
    }
};
