#include "core/AssetId.h"

#include <cstdio>
#include <random>

namespace Engine {
namespace {

std::mt19937_64& generator() {
    // Seeded once from the operating system's random source. mt19937_64 is
    // not suitable for secrets, but ids only need to be unique, not secret.
    static std::mt19937_64 engine = [] {
        std::random_device device;
        std::seed_seq seed{device(), device(), device(), device(), device(), device(), device(), device()};
        return std::mt19937_64(seed);
    }();
    return engine;
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

} // namespace

AssetId AssetId::generate() {
    AssetId id;
    do {
        id.high = generator()();
        id.low = generator()();
        // Mark it as a version 4 (random) GUID, variant 1, as RFC 4122 asks:
        // other tools then recognize what kind of id it is.
        id.high = (id.high & ~0xF000ull) | 0x4000ull;
        id.low = (id.low & ~(0xC000000000000000ull)) | 0x8000000000000000ull;
    } while (id.isNull()); // can't happen with the bits above, but be explicit
    return id;
}

std::string AssetId::toString() const {
    char text[37];
    std::snprintf(text, sizeof text, "%08x-%04x-%04x-%04x-%012llx", static_cast<unsigned>(high >> 32),
                  static_cast<unsigned>((high >> 16) & 0xFFFF), static_cast<unsigned>(high & 0xFFFF),
                  static_cast<unsigned>(low >> 48), static_cast<unsigned long long>(low & 0xFFFFFFFFFFFFull));
    return text;
}

std::optional<AssetId> AssetId::parse(const std::string& text) {
    if (text.size() != 36) {
        return std::nullopt;
    }
    AssetId id;
    int digits = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (text[i] != '-') {
                return std::nullopt;
            }
            continue;
        }
        const int v = hexValue(text[i]);
        if (v < 0) {
            return std::nullopt;
        }
        // The first 16 hex digits fill `high`, the last 16 fill `low`.
        std::uint64_t& half = digits < 16 ? id.high : id.low;
        half = (half << 4) | static_cast<std::uint64_t>(v);
        ++digits;
    }
    return id;
}

} // namespace Engine
