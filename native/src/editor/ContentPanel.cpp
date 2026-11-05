// The content browser: every asset the registry knows, grouped by folder.
//
//   drag a model into the viewport      a new entity with that mesh
//   drag a texture onto an object       paints it (or onto an inspector slot)
//   double-click a scene                opens it
//
// Drag-and-drop carries the asset's id ("XE_ASSET" payload), never its path,
// for the same reason scenes do: the id is what stays the same.

#include "editor/EditorApp.h"

#include <imgui.h>

#include <filesystem>
#include <map>

namespace Engine {
namespace {

const char* typeTag(AssetType type) {
    switch (type) {
    case AssetType::Texture: return "[tex]  ";
    case AssetType::Mesh: return "[mesh] ";
    case AssetType::Scene: return "[scene]";
    }
    return "[?]    ";
}

ImVec4 typeColor(AssetType type) {
    switch (type) {
    case AssetType::Texture: return ImVec4(0.55f, 0.8f, 1.0f, 1.0f);
    case AssetType::Mesh: return ImVec4(0.6f, 1.0f, 0.6f, 1.0f);
    case AssetType::Scene: return ImVec4(1.0f, 0.8f, 0.45f, 1.0f);
    }
    return ImVec4(1, 1, 1, 1);
}

} // namespace

void EditorApp::drawContent() {
    if (!ImGui::Begin("Content")) {
        ImGui::End();
        return;
    }
    if (ImGui::Button("Rescan")) registry_.scan();
    ImGui::SameLine();
    ImGui::TextDisabled("%s   (drag a model into the viewport, a texture onto an object)", registry_.root().c_str());
    ImGui::Separator();

    // Group by folder: "models/barrel.obj" goes under "models".
    std::map<std::string, std::vector<const AssetRecord*>> folders;
    for (const AssetRecord* record : registry_.all()) {
        folders[std::filesystem::path(record->path).parent_path().generic_string()].push_back(record);
    }
    for (const auto& [folder, records] : folders) {
        if (!ImGui::TreeNodeEx(folder.empty() ? "(top)" : folder.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) continue;
        for (const AssetRecord* record : records) {
            ImGui::PushID(record->path.c_str());
            ImGui::TextColored(typeColor(record->type), "%s", typeTag(record->type));
            ImGui::SameLine();
            const std::string name = std::filesystem::path(record->path).filename().string();
            const bool current = record->path == scenePath_;
            ImGui::Selectable(name.c_str(), current, ImGuiSelectableFlags_AllowDoubleClick);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s\nid %s", record->path.c_str(), record->id.toString().c_str());
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && record->type == AssetType::Scene) {
                    const std::string path = record->path;
                    whenChangesHandled([this, path] { openScene(path); });
                }
            }
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("XE_ASSET", &record->id, sizeof record->id);
                ImGui::Text("%s", record->path.c_str());
                ImGui::EndDragDropSource();
            }
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
    ImGui::End();
}

} // namespace Engine
