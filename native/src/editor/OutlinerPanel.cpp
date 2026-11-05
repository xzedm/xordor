// The outliner: the scene's hierarchy as a tree.
//
//   click          select
//   drag onto      make it a child of the entity you drop it on
//   drag below     (the empty space under the tree) move it to the top level
//   right click    create a child, duplicate, delete
//
// Every change is a command, so it can be undone. Changes are queued in
// deferred_ and run after the tree is drawn: reparenting a node while ImGui
// is in the middle of walking its siblings would change the list being walked.

#include "editor/EditorApp.h"

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace Engine {
namespace {

const char* EntityPayload = "XE_ENTITY";

} // namespace

void EditorApp::drawOutliner() {
    if (!ImGui::Begin("Outliner")) {
        ImGui::End();
        return;
    }
    if (ImGui::Button("+ Entity")) ImGui::OpenPopup("CreateTop");
    if (ImGui::BeginPopup("CreateTop")) {
        drawCreateMenuItems(0);
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%zu entities", doc_.scene.entityCount());
    ImGui::Separator();

    for (Entity root : doc_.scene.roots()) {
        drawOutlinerNode(root);
    }

    // The empty space below the tree: a drop target for "no parent", and a
    // click there clears the selection.
    const ImVec2 rest = ImGui::GetContentRegionAvail();
    ImGui::InvisibleButton("##outliner-empty", ImVec2(std::max(rest.x, 1.0f), std::max(rest.y, 24.0f)));
    if (ImGui::IsItemClicked()) select(0);
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(EntityPayload)) {
            SceneId dragged;
            std::memcpy(&dragged, payload->Data, sizeof dragged);
            deferred_.push_back([this, dragged] {
                if (!doc_.scene.parentOf(doc_.scene.findById(dragged)).isNull()) {
                    execute(std::make_unique<ReparentCommand>(doc_, dragged, 0, 0));
                }
            });
        }
        ImGui::EndDragDropTarget();
    }
    if (ImGui::BeginPopupContextItem("##outliner-empty-menu")) {
        drawCreateMenuItems(0);
        ImGui::EndPopup();
    }
    ImGui::End();
}

void EditorApp::drawOutlinerNode(Entity e) {
    const SceneId id = doc_.scene.idOf(e);
    const std::vector<Entity>& children = doc_.scene.childrenOf(e);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
    if (children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
    if (id == doc_.selected) flags |= ImGuiTreeNodeFlags_Selected;

    // The SceneId is the widget's id: stable even when names repeat or change.
    const bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<std::uintptr_t>(id)), flags, "%s",
                                        doc_.scene.nameOf(e).c_str());
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) select(id);

    // Drag this entity...
    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload(EntityPayload, &id, sizeof id);
        ImGui::Text("Move %s", doc_.scene.nameOf(e).c_str());
        ImGui::EndDragDropSource();
    }
    // ...or drop another one onto it, to make it a child.
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(EntityPayload)) {
            SceneId dragged;
            std::memcpy(&dragged, payload->Data, sizeof dragged);
            deferred_.push_back([this, dragged, id] {
                const Entity child = doc_.scene.findById(dragged);
                const Entity parent = doc_.scene.findById(id);
                if (dragged == id || doc_.scene.isDescendantOf(parent, child)) {
                    ENGINE_WARN("Can't put ", doc_.scene.nameOf(child), " inside itself");
                    return;
                }
                execute(std::make_unique<ReparentCommand>(doc_, dragged, id, doc_.scene.childrenOf(parent).size()));
            });
        }
        ImGui::EndDragDropTarget();
    }
    if (ImGui::BeginPopupContextItem()) {
        select(id);
        if (ImGui::BeginMenu("Create Child")) {
            drawCreateMenuItems(id);
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Duplicate", "Ctrl+D")) duplicateSelected();
        if (ImGui::MenuItem("Delete", "Del")) deleteSelected();
        if (!doc_.scene.parentOf(e).isNull() && ImGui::MenuItem("Move to Top Level")) {
            deferred_.push_back([this, id] { execute(std::make_unique<ReparentCommand>(doc_, id, 0, 0)); });
        }
        ImGui::EndPopup();
    }

    if (open) {
        // Copy: the list must not change under us (actions are deferred anyway,
        // but a copy makes the loop safe by construction).
        const std::vector<Entity> kids = children;
        for (Entity child : kids) {
            drawOutlinerNode(child);
        }
        ImGui::TreePop();
    }
}

} // namespace Engine
