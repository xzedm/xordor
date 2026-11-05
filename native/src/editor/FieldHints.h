#pragma once
// How the inspector shows a component's fields.
//
// The inspector is *generated*, not hand-written per component. It asks the
// scene serializer for a component as JSON, which is the same data that goes
// into scene files, and draws a widget for each field from its JSON type:
//
//   number             -> drag box        true / false      -> checkbox
//   [x, y] / [x, y, z] -> 2 or 3 boxes    text              -> text box
//   {"id", "path"}     -> asset slot (pick from a list, or drop a file on it)
//
// Editing writes the JSON back through the serializer. So every component the
// serializer knows, including an app's own, gets an inspector for free, and
// what you can edit is exactly what gets saved.
//
// Types alone can't tell a color from a position, so a small table of hints
// adds what's missing: "baseColor is a color", "shape is box or sphere".
// This is the editor's "reflection": data describing data, used to build UI.

#include <string>
#include <vector>

namespace Engine {

enum class FieldWidget {
    Auto,         // from the JSON type
    Color,        // [r, g, b] with a color picker
    Degrees,      // angles in degrees (dragged `speed` degrees per pixel)
    MeshAsset,    // asset slot accepting meshes
    TextureAsset, // asset slot accepting textures
    Choice,       // one of `choices`
    ReadOnly,     // shown, not editable
};

struct FieldHint {
    FieldWidget widget = FieldWidget::Auto;
    float speed = 0.01f; // drag sensitivity: units per pixel
    float min = 0.0f;    // min == max means "no limit"
    float max = 0.0f;
    std::vector<std::string> choices;
};

// The hint for a field such as ("meshRenderer", "material.baseColor"), or
// the default (Auto) when there's none.
const FieldHint& fieldHint(const std::string& componentKey, const std::string& fieldPath);

// "meshRenderer" -> "Mesh Renderer", "halfExtents" -> "Half Extents".
std::string prettyLabel(const std::string& key);

} // namespace Engine
