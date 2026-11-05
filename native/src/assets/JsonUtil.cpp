#include "assets/JsonUtil.h"

#include <algorithm>
#include <cmath>

namespace Engine {
namespace {

// The value at `key`, or nullptr when it's missing or null.
const Json* field(const Json& object, const char* key) {
    if (!object.is_object()) {
        return nullptr;
    }
    const auto it = object.find(key);
    return it == object.end() || it->is_null() ? nullptr : &*it;
}

[[noreturn]] void wrongType(const char* key, const char* expected, const Json& value) {
    throw JsonError(std::string("\"") + key + "\" should be " + expected + ", but is " + value.dump());
}

template <int N>
glm::vec<N, float> readVector(const Json& object, const char* key, const glm::vec<N, float>& fallback) {
    const Json* v = field(object, key);
    if (!v) {
        return fallback;
    }
    if (!v->is_array() || v->size() != N) {
        wrongType(key, N == 2 ? "an array of 2 numbers" : "an array of 3 numbers", *v);
    }
    glm::vec<N, float> result;
    for (int i = 0; i < N; ++i) {
        const Json& component = (*v)[static_cast<size_t>(i)];
        if (!component.is_number()) {
            wrongType(key, "an array of numbers", *v);
        }
        result[i] = component.get<float>();
    }
    return result;
}

} // namespace

double tidy(float value, int decimals) {
    const double scale = std::pow(10.0, decimals);
    const double r = std::round(static_cast<double>(value) * scale) / scale;
    return r == 0.0 ? 0.0 : r; // no "-0.0"
}
Json jsonArray(const Vec2& v, int decimals) { return Json::array({tidy(v.x, decimals), tidy(v.y, decimals)}); }
Json jsonArray(const Vec3& v, int decimals) {
    return Json::array({tidy(v.x, decimals), tidy(v.y, decimals), tidy(v.z, decimals)});
}

namespace {

bool isFlatArray(const Json& array) {
    return std::none_of(array.begin(), array.end(), [](const Json& e) { return e.is_structured(); });
}

void writePretty(std::string& out, const Json& value, int indent) {
    const std::string pad(static_cast<size_t>(indent), ' ');
    const std::string inner(static_cast<size_t>(indent) + 2, ' ');
    if (value.is_object() && !value.empty()) {
        out += "{\n";
        size_t i = 0;
        for (const auto& [key, item] : value.items()) {
            out += inner + Json(key).dump() + ": ";
            writePretty(out, item, indent + 2);
            out += ++i < value.size() ? ",\n" : "\n";
        }
        out += pad + "}";
    } else if (value.is_array() && !value.empty() && !isFlatArray(value)) {
        out += "[\n";
        for (size_t i = 0; i < value.size(); ++i) {
            out += inner;
            writePretty(out, value[i], indent + 2);
            out += i + 1 < value.size() ? ",\n" : "\n";
        }
        out += pad + "]";
    } else if (value.is_array()) {
        out += "[";
        for (size_t i = 0; i < value.size(); ++i) {
            out += (i ? ", " : "") + value[i].dump();
        }
        out += "]";
    } else {
        out += value.dump(); // numbers, strings, true/false/null, {} and []
    }
}

} // namespace

std::string prettyJson(const Json& value) {
    std::string out;
    writePretty(out, value, 0);
    return out + "\n";
}

float readFloat(const Json& object, const char* key, float fallback) {
    const Json* v = field(object, key);
    if (!v) return fallback;
    if (!v->is_number()) wrongType(key, "a number", *v);
    return v->get<float>();
}

bool readBool(const Json& object, const char* key, bool fallback) {
    const Json* v = field(object, key);
    if (!v) return fallback;
    if (!v->is_boolean()) wrongType(key, "true or false", *v);
    return v->get<bool>();
}

std::string readString(const Json& object, const char* key, const std::string& fallback) {
    const Json* v = field(object, key);
    if (!v) return fallback;
    if (!v->is_string()) wrongType(key, "a string", *v);
    return v->get<std::string>();
}

Vec2 readVec2(const Json& object, const char* key, const Vec2& fallback) { return readVector<2>(object, key, fallback); }
Vec3 readVec3(const Json& object, const char* key, const Vec3& fallback) { return readVector<3>(object, key, fallback); }

AssetId readAssetId(const Json& object, const char* key) {
    const Json* v = field(object, key);
    if (!v) return {};
    if (!v->is_string()) wrongType(key, "an asset id string", *v);
    const auto id = AssetId::parse(v->get<std::string>());
    if (!id) wrongType(key, "an asset id like \"3f2b8c1e-9d4a-4f6b-8e21-c0ffee000001\"", *v);
    return *id;
}

} // namespace Engine
