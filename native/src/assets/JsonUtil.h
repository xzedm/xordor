#pragma once
// Small helpers for reading and writing engine types as JSON.
//
// Reading is forgiving on purpose: a missing field keeps its default value,
// so a file written by an older version (or typed by hand) still loads. A
// field that is present but wrong (a string where a number belongs) throws
// JsonError naming the field, and the loader turns that into a readable
// error message instead of a crash.

#include "core/AssetId.h"
#include "core/Math.h"

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>

namespace Engine {

// ordered_json keeps keys in the order they were added (plain nlohmann::json
// sorts them alphabetically), so files read top to bottom the way the code
// writes them: "id", then "name", then the components.
using Json = nlohmann::ordered_json;

struct JsonError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Writing. Floats like 0.3f become 0.30000001192092896 when stored as
// doubles; these round to 6 decimals so files stay readable and diffs small.
// (A micrometer, or a millionth of a degree, is far below anything visible.)
// Angles get 4 decimals: float math leaves noise like 35.000004 degrees, and
// a ten-thousandth of a degree is still far below anything visible.
double tidy(float value, int decimals = 6);
Json jsonArray(const Vec2& v, int decimals = 6);
Json jsonArray(const Vec3& v, int decimals = 6);

// Like json.dump(2), but short arrays of plain values stay on one line:
//   "position": [1.0, 3.0, 7.0]     instead of five lines.
// The sandbox scene is about half the length, and much easier to read and diff.
std::string prettyJson(const Json& value);

// Each read*() returns `fallback` when the key is missing (or null).
float readFloat(const Json& object, const char* key, float fallback);
bool readBool(const Json& object, const char* key, bool fallback);
std::string readString(const Json& object, const char* key, const std::string& fallback);
Vec2 readVec2(const Json& object, const char* key, const Vec2& fallback);
Vec3 readVec3(const Json& object, const char* key, const Vec3& fallback);
AssetId readAssetId(const Json& object, const char* key);

} // namespace Engine
