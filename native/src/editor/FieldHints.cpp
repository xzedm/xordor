#include "editor/FieldHints.h"

#include <cctype>
#include <map>

namespace Engine {
namespace {

FieldHint hint(FieldWidget widget, float speed = 0.01f, float min = 0.0f, float max = 0.0f,
               std::vector<std::string> choices = {}) {
    return FieldHint{widget, speed, min, max, std::move(choices)};
}

const std::map<std::string, FieldHint>& table() {
    // Keyed by "component.field.path". Anything not listed uses Auto.
    static const std::map<std::string, FieldHint> hints = {
        {"transform.position", hint(FieldWidget::Auto, 0.01f)},
        {"transform.rotation", hint(FieldWidget::Degrees, 0.5f)},
        {"transform.scale", hint(FieldWidget::Auto, 0.01f, 0.001f, 1000.0f)},
        {"meshRenderer.mesh", hint(FieldWidget::MeshAsset)},
        {"meshRenderer.material.albedo", hint(FieldWidget::TextureAsset)},
        {"meshRenderer.material.baseColor", hint(FieldWidget::Color)},
        {"meshRenderer.material.specular", hint(FieldWidget::Auto, 0.005f, 0.0f, 1.0f)},
        {"meshRenderer.material.shininess", hint(FieldWidget::Auto, 0.5f, 1.0f, 256.0f)},
        {"meshRenderer.material.uvScale", hint(FieldWidget::Auto, 0.05f, 0.01f, 1000.0f)},
        {"camera.fovDegrees", hint(FieldWidget::Degrees, 0.2f, 5.0f, 170.0f)},
        {"camera.near", hint(FieldWidget::Auto, 0.001f, 0.001f, 100.0f)},
        {"camera.far", hint(FieldWidget::Auto, 1.0f, 1.0f, 100000.0f)},
        {"light.color", hint(FieldWidget::Color)},
        {"light.intensity", hint(FieldWidget::Auto, 0.01f, 0.0f, 100.0f)},
        {"collider.shape", hint(FieldWidget::Choice, 0.0f, 0.0f, 0.0f, {"box", "sphere"})},
        {"collider.halfExtents", hint(FieldWidget::Auto, 0.01f, 0.0f, 1000.0f)},
        {"collider.radius", hint(FieldWidget::Auto, 0.01f, 0.0f, 1000.0f)},
        // The asset path inside a reference is a note for people; the id decides.
        {"meshRenderer.mesh.path", hint(FieldWidget::ReadOnly)},
        {"meshRenderer.material.albedo.path", hint(FieldWidget::ReadOnly)},
    };
    return hints;
}

} // namespace

const FieldHint& fieldHint(const std::string& componentKey, const std::string& fieldPath) {
    static const FieldHint automatic;
    const auto it = table().find(componentKey + "." + fieldPath);
    return it == table().end() ? automatic : it->second;
}

std::string prettyLabel(const std::string& key) {
    // Split camelCase into words and capitalize the first letter of each.
    std::string out;
    for (size_t i = 0; i < key.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(key[i]);
        if (i == 0) {
            out += static_cast<char>(std::toupper(c));
        } else if (std::isupper(c) && !std::isupper(static_cast<unsigned char>(key[i - 1]))) {
            out += ' ';
            out += static_cast<char>(c);
        } else {
            out += static_cast<char>(c);
        }
    }
    return out;
}

} // namespace Engine
