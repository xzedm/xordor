// The inspector: every component of the selected entity, as editable fields.
//
// Nothing here is written per component type. For each component we ask the
// serializer for its JSON, draw a widget for each field (editField, using
// FieldHints for colors, choices and asset slots), and if anything changed,
// load the edited JSON back into the component. Whatever the serializer can
// save, the inspector can edit, including components an app registered itself
// and components no serializer knows (kept as plain JSON).
//
// Undo for live edits. Dragging a number changes it every frame, but undo
// should be one step, not sixty. So the first change of an interaction
// remembers the component as it was (noteEdit), the changes apply live, and
// when no widget is active any more (the mouse was released, Enter pressed)
// commitEdit pushes a single command with the before and after JSON.
// (The gizmo uses the same pair: it notes the transform when a drag starts
// and commits when it ends.)

#include "editor/EditorApp.h"
#include "editor/FieldHints.h"

#include <imgui.h>

#include <cstdio>
#include <cstring>

namespace Engine {

// ---------------------------------------------------------------- live edits

void EditorApp::noteEdit(Entity e, const std::string& key, const Json& before) {
    const SceneId id = doc_.scene.idOf(e);
    if (pending_.active && (pending_.entity != id || pending_.key != key)) {
        commitEdit(true); // a different component: the previous edit is finished
    }
    if (!pending_.active) {
        pending_ = {true, id, key, before};
    }
}

void EditorApp::commitEdit(bool force) {
    if (!pending_.active) return;
    // Still dragging or typing? The gizmo isn't an ImGui widget, so it's asked separately.
    if (!force && ((ImGui::GetCurrentContext() && ImGui::IsAnyItemActive()) || gizmo_.isActive())) return;
    pending_.active = false;
    const Entity e = doc_.scene.findById(pending_.entity);
    if (e.isNull()) return;
    Json after = serializer_.saveComponent(doc_.scene, e, pending_.key);
    if (after == pending_.before) return; // dragged and put back: nothing to undo
    undo_.pushApplied(std::make_unique<ChangeComponentCommand>(pending_.entity, pending_.key, pending_.before, std::move(after),
                                                               "Edit " + prettyLabel(pending_.key) + " of " + doc_.scene.nameOf(e)));
}

// ---------------------------------------------------------------- the panel

void EditorApp::drawInspector() {
    if (!ImGui::Begin("Inspector")) {
        ImGui::End();
        return;
    }
    const Entity e = selected();
    if (e.isNull()) {
        ImGui::TextDisabled("Select an entity in the viewport or the outliner.");
        ImGui::End();
        return;
    }
    const SceneId id = doc_.selected;

    // Name: edited in a buffer, committed as one rename when the box loses focus.
    if (renameBufferFor_ != id) {
        std::snprintf(renameBuffer_, sizeof renameBuffer_, "%s", doc_.scene.nameOf(e).c_str());
        renameBufferFor_ = id;
    }
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##name", renameBuffer_, sizeof renameBuffer_);
    if (ImGui::IsItemDeactivatedAfterEdit() && doc_.scene.nameOf(e) != renameBuffer_) {
        execute(std::make_unique<RenameCommand>(id, doc_.scene.nameOf(e), renameBuffer_));
    }
    ImGui::TextDisabled("Scene id %llu%s", static_cast<unsigned long long>(id),
                        doc_.scene.parentOf(e).isNull() ? "" : ("   child of " + doc_.scene.nameOf(doc_.scene.parentOf(e))).c_str());
    ImGui::Separator();

    for (const std::string& key : serializer_.componentKeysOf(doc_.scene, e)) {
        drawComponent(e, key);
    }

    ImGui::Spacing();
    if (ImGui::Button("Add Component", ImVec2(-1.0f, 0.0f))) ImGui::OpenPopup("AddComponent");
    if (ImGui::BeginPopup("AddComponent")) {
        for (const std::string& key : serializer_.componentKeys()) {
            if (serializer_.hasComponent(doc_.scene, e, key)) continue;
            if (ImGui::MenuItem(prettyLabel(key).c_str())) {
                // An empty object loads as all defaults.
                execute(std::make_unique<ChangeComponentCommand>(id, key, nullptr, Json::object(),
                                                                 "Add " + prettyLabel(key)));
            }
        }
        ImGui::EndPopup();
    }
    ImGui::End();
}

void EditorApp::drawComponent(Entity e, const std::string& key) {
    ImGui::PushID(key.c_str());
    // Transform can't be removed: every entity has a place in the world.
    bool keep = true;
    const bool removable = key != "transform";
    const bool open = ImGui::CollapsingHeader(prettyLabel(key).c_str(), removable ? &keep : nullptr,
                                              ImGuiTreeNodeFlags_DefaultOpen);
    if (!serializer_.isRegistered(key) && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("No serializer is registered for \"%s\" here: it's kept and edited as plain JSON.", key.c_str());
    }
    if (!keep) {
        const SceneId id = doc_.scene.idOf(e);
        const Json before = serializer_.saveComponent(doc_.scene, e, key);
        deferred_.push_back([this, id, key, before] {
            execute(std::make_unique<ChangeComponentCommand>(id, key, before, nullptr, "Remove " + prettyLabel(key)));
        });
    } else if (open) {
        const Json before = serializer_.saveComponent(doc_.scene, e, key);
        Json value = before;
        bool changed = false;
        if (value.is_object()) {
            for (auto& item : value.items()) {
                changed |= editField(key, item.key(), prettyLabel(item.key()), item.value());
            }
        } else {
            changed |= editField(key, "", prettyLabel(key), value);
        }
        if (changed) {
            noteEdit(e, key, before);
            try {
                serializer_.loadComponent(doc_.scene, e, key, value);
            } catch (const std::exception& error) {
                ENGINE_WARN("Edit not applied: ", error.what());
            }
        }
    }
    ImGui::PopID();
}

// ---------------------------------------------------------------- widgets from JSON

bool EditorApp::editField(const std::string& componentKey, const std::string& path, const std::string& label, Json& value) {
    const FieldHint& hint = fieldHint(componentKey, path);
    ImGui::PushID(path.c_str());
    bool changed = false;
    const float minimum = hint.min, maximum = hint.max; // equal = no limits
    const auto isNumberArray = [&](std::size_t n) {
        if (!value.is_array() || value.size() != n) return false;
        for (const Json& v : value) if (!v.is_number()) return false;
        return true;
    };

    if (hint.widget == FieldWidget::ReadOnly) {
        ImGui::TextDisabled("%s: %s", label.c_str(), value.is_string() ? value.get<std::string>().c_str() : value.dump().c_str());
    } else if (hint.widget == FieldWidget::MeshAsset || hint.widget == FieldWidget::TextureAsset) {
        changed = assetSlot(label, value, hint.widget == FieldWidget::MeshAsset ? AssetType::Mesh : AssetType::Texture);
    } else if (value.is_object() && value.contains("id") && value.contains("path")) {
        // An asset reference without a hint (in an app's own component): guess the type from the file.
        const AssetRecord* record = registry_.find(readAssetId(value, "id"));
        changed = assetSlot(label, value, record ? record->type : AssetType::Texture);
    } else if (value.is_object()) {
        if (ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            for (auto& item : value.items()) {
                changed |= editField(componentKey, path + "." + item.key(), prettyLabel(item.key()), item.value());
            }
            ImGui::TreePop();
        }
    } else if (value.is_boolean()) {
        bool b = value.get<bool>();
        if (ImGui::Checkbox(label.c_str(), &b)) {
            value = b;
            changed = true;
        }
    } else if (value.is_number()) {
        float f = value.get<float>();
        if (ImGui::DragFloat(label.c_str(), &f, hint.speed, minimum, maximum, "%.3f",
                             minimum != maximum ? ImGuiSliderFlags_AlwaysClamp : 0)) {
            value = tidy(f);
            changed = true;
        }
    } else if (value.is_string() && !hint.choices.empty()) {
        const std::string current = value.get<std::string>();
        if (ImGui::BeginCombo(label.c_str(), current.c_str())) {
            for (const std::string& choice : hint.choices) {
                if (ImGui::Selectable(choice.c_str(), choice == current) && choice != current) {
                    value = choice;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
    } else if (value.is_string()) {
        char buffer[256];
        std::snprintf(buffer, sizeof buffer, "%s", value.get<std::string>().c_str());
        if (ImGui::InputText(label.c_str(), buffer, sizeof buffer)) {
            value = std::string(buffer);
            changed = true;
        }
    } else if (hint.widget == FieldWidget::Color && isNumberArray(3)) {
        float c[3] = {value[0].get<float>(), value[1].get<float>(), value[2].get<float>()};
        if (ImGui::ColorEdit3(label.c_str(), c, ImGuiColorEditFlags_Float)) {
            value = Json::array({tidy(c[0]), tidy(c[1]), tidy(c[2])});
            changed = true;
        }
    } else if (isNumberArray(2) || isNumberArray(3) || isNumberArray(4)) {
        float v[4] = {};
        const int n = static_cast<int>(value.size());
        for (int i = 0; i < n; ++i) v[i] = value[static_cast<size_t>(i)].get<float>();
        if (ImGui::DragScalarN(label.c_str(), ImGuiDataType_Float, v, n, hint.speed, minimum != maximum ? &hint.min : nullptr,
                               minimum != maximum ? &hint.max : nullptr, hint.widget == FieldWidget::Degrees ? "%.1f°" : "%.3f")) {
            Json out = Json::array();
            for (int i = 0; i < n; ++i) out.push_back(tidy(v[i], hint.widget == FieldWidget::Degrees ? 4 : 6));
            value = std::move(out);
            changed = true;
        }
    } else {
        // Anything else (null, an array of objects...): show it, don't guess how to edit it.
        ImGui::TextDisabled("%s: %s", label.c_str(), value.dump().c_str());
    }
    ImGui::PopID();
    return changed;
}

std::string EditorApp::assetLabel(AssetId id) const {
    if (id.isNull()) return "(none)";
    if (id == BuiltinAssets::Cube) return "builtin: cube";
    if (id == BuiltinAssets::Plane) return "builtin: plane";
    if (id == BuiltinAssets::Sphere) return "builtin: sphere";
    const AssetRecord* record = registry_.find(id);
    return record ? record->path : "(missing " + id.toString().substr(0, 8) + "...)";
}

bool EditorApp::assetSlot(const std::string& label, Json& value, AssetType type) {
    const AssetId current = value.is_object() ? readAssetId(value, "id") : AssetId{};
    bool changed = false;
    AssetId chosen = current;
    if (ImGui::BeginCombo(label.c_str(), assetLabel(current).c_str())) {
        if (type == AssetType::Texture && ImGui::Selectable("(none)", current.isNull())) chosen = AssetId{};
        if (type == AssetType::Mesh) {
            for (AssetId builtin : {BuiltinAssets::Cube, BuiltinAssets::Plane, BuiltinAssets::Sphere}) {
                if (ImGui::Selectable(assetLabel(builtin).c_str(), builtin == current)) chosen = builtin;
            }
        }
        for (const AssetRecord* record : registry_.all()) {
            if (record->type == type && ImGui::Selectable(record->path.c_str(), record->id == current)) chosen = record->id;
        }
        ImGui::EndCombo();
    }
    // Dropping a file from the Content panel onto the slot works too.
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("XE_ASSET")) {
            AssetId dropped;
            std::memcpy(&dropped, payload->Data, sizeof dropped);
            const AssetRecord* record = registry_.find(dropped);
            if (record && record->type == type) {
                chosen = dropped;
            } else {
                ENGINE_WARN("That slot takes a ", AssetRegistry::typeName(type));
            }
        }
        ImGui::EndDragDropTarget();
    }
    if (chosen != current) {
        value = serializer_.assetRef(chosen);
        changed = true;
    }
    return changed;
}

} // namespace Engine
