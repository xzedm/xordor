// The viewport: the scene seen through the editor's own camera.
//
// Each frame:
//   1. size the offscreen framebuffer to the panel,
//   2. draw the scene into it (SceneRenderer::renderView with the editor camera),
//   3. show its color texture as an ImGui image,
//   4. draw the gizmo over the image and handle the mouse: fly, click to
//      select, drag the gizmo, drop assets.
//
// The editor camera is not an entity, so moving it never changes the scene
// (or the undo history), and it isn't saved.

#include "editor/EditorApp.h"

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace Engine {

CameraView EditorApp::editorView(float aspect) const {
    Camera view = camera_;
    view.aspect = aspect;
    return view.toView();
}

void EditorApp::drawViewport() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const bool visible = ImGui::Begin("Viewport");
    ImGui::PopStyleVar();
    if (!visible) {
        viewportHovered_ = false;
        ImGui::End();
        return;
    }

    // ---- toolbar
    ImGui::SetCursorPos(ImVec2(8.0f, ImGui::GetCursorPosY() + 4.0f));
    int mode = static_cast<int>(gizmoMode_);
    ImGui::RadioButton("Move (W)", &mode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Rotate (E)", &mode, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Scale (R)", &mode, 2);
    gizmoMode_ = static_cast<GizmoMode>(mode);
    ImGui::SameLine(0.0f, 20.0f);
    bool local = gizmoSpace_ == GizmoSpace::Local;
    if (ImGui::Checkbox("Local axes", &local)) gizmoSpace_ = local ? GizmoSpace::Local : GizmoSpace::World;
    ImGui::SameLine(0.0f, 20.0f);
    ImGui::Checkbox("Snap", &snap_.enabled);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("0.5 m, 15 degrees, 0.1 scale steps (hold Ctrl to snap once)");
    ImGui::SameLine(0.0f, 20.0f);
    ImGui::Checkbox("Colliders", &sceneRenderer_->showColliders);

    // ---- the scene, drawn into the framebuffer at the panel's size
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const int width = std::max(1, static_cast<int>(avail.x));
    const int height = std::max(1, static_cast<int>(avail.y));
    viewportTarget_->resize(width, height);
    viewportTarget_->bind();
    renderer_->setViewport(width, height);
    const CameraView view = editorView(static_cast<float>(width) / static_cast<float>(height));
    sceneRenderer_->selected = selected();
    sceneRenderer_->renderView(doc_.scene, *assets_, view, 1.0f);
    Framebuffer::bindDefault();

    // OpenGL's row 0 is the bottom, ImGui's top: flip v when showing it.
    const ImVec2 imagePos = ImGui::GetCursorScreenPos();
    ImGui::Image(static_cast<ImTextureID>(static_cast<std::intptr_t>(viewportTarget_->colorTexture())),
                 ImVec2(static_cast<float>(width), static_cast<float>(height)), ImVec2(0, 1), ImVec2(1, 0));
    viewportHovered_ = ImGui::IsItemHovered();
    const Vec2 pos(imagePos.x, imagePos.y);
    const Vec2 size(static_cast<float>(width), static_cast<float>(height));
    const ImGuiIO& io = ImGui::GetIO();
    const Vec2 mouse(io.MousePos.x, io.MousePos.y);

    // ---- drop an asset from the Content panel
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("XE_ASSET")) {
            AssetId id;
            std::memcpy(&id, payload->Data, sizeof id);
            const AssetRecord* record = registry_.find(id);
            const Ray ray = GizmoMath::screenToRay(mouse, glm::inverse(view.viewProjection()), pos, size);
            if (record && record->type == AssetType::Mesh) {
                // A model becomes a new entity, standing where the mouse points.
                const std::string name = std::filesystem::path(record->path).stem().string();
                createEntity(name, 0,
                             Json{{"transform", {{"position", jsonArray(placementPoint(ray))}}},
                                  {"meshRenderer", {{"mesh", serializer_.assetRef(id)}}}});
            } else if (record && record->type == AssetType::Texture) {
                // A texture paints whatever it's dropped on.
                const Entity target = pickEntity(doc_.scene, *assets_, ray);
                if (doc_.scene.has<MeshRenderer>(target)) {
                    const Json before = serializer_.saveComponent(doc_.scene, target, "meshRenderer");
                    Json after = before;
                    after["material"]["albedo"] = serializer_.assetRef(id);
                    execute(std::make_unique<ChangeComponentCommand>(doc_.scene.idOf(target), "meshRenderer", before,
                                                                     after, "Texture " + doc_.scene.nameOf(target)));
                }
            } else if (record && record->type == AssetType::Scene) {
                const std::string path = record->path;
                whenChangesHandled([this, path] { openScene(path); });
            }
        }
        ImGui::EndDragDropTarget();
    }

    // ---- fly: right mouse held over the viewport (keeps flying until released)
    if (viewportHovered_ && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) flying_ = true;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) flying_ = false;
    updateEditorCamera(frameSeconds_);

    // ---- gizmo on the selection
    const Entity sel = selected();
    if (!sel.isNull() && !flying_) {
        GizmoViewport gv{view, pos, size};
        GizmoSnap snap = snap_;
        snap.enabled = snap_.enabled || io.KeyCtrl;
        Mat4 world = doc_.scene.worldMatrix(sel);
        // The transform as it is now: if a drag starts this frame, this is
        // what undo goes back to.
        const Json before = serializer_.saveComponent(doc_.scene, sel, "transform");
        const bool changed = gizmo_.manipulate(gv, gizmoMode_, gizmoSpace_, snap, world);
        if (gizmo_.dragStarted()) noteEdit(sel, "transform", before);
        if (changed) applyWorldMatrix(sel, world);
        if (gizmo_.dragEnded()) commitEdit(true);
    }

    // ---- click to select (not when the click grabbed the gizmo)
    if (viewportHovered_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !gizmo_.isHovered() && !gizmo_.isActive()) {
        pickAt(mouse, pos, size);
    }

    // ---- a small overlay with numbers
    char overlay[160];
    std::snprintf(overlay, sizeof overlay, "%.0f FPS   %zu entities   %d draws   camera %.1f m/s", stats().fps,
                  doc_.scene.entityCount(), renderer_->stats().drawCalls, flyController_.speed);
    ImGui::GetWindowDrawList()->AddText(ImVec2(imagePos.x + 8.0f, imagePos.y + 6.0f), IM_COL32(255, 255, 255, 200), overlay);
    if (sel.isNull()) {
        ImGui::GetWindowDrawList()->AddText(ImVec2(imagePos.x + 8.0f, imagePos.y + 24.0f), IM_COL32(255, 255, 255, 140),
                                            "Click to select. Right mouse + WASD to fly.");
    }
    ImGui::End();
}

void EditorApp::updateEditorCamera(float dt) {
    // The fly controller reads our own Input (which still receives every event,
    // since ImGui's GLFW backend passes them on), and only moves while flying.
    if (flying_) {
        flyController_.update(camera_, input(), dt);
    }
    if (flying_ != window().cursorCaptured()) {
        window().setCursorCaptured(flying_);
        // While the cursor is hidden and captured, ImGui mustn't react to it.
        ImGuiIO& io = ImGui::GetIO();
        if (flying_) {
            io.ConfigFlags |= ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange;
        } else {
            io.ConfigFlags &= ~(ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange);
        }
    }
}

void EditorApp::pickAt(const Vec2& mouse, const Vec2& viewportPos, const Vec2& viewportSize) {
    const CameraView view = editorView(viewportSize.x / viewportSize.y);
    const Ray ray = GizmoMath::screenToRay(mouse, glm::inverse(view.viewProjection()), viewportPos, viewportSize);
    const Entity hit = pickEntity(doc_.scene, *assets_, ray);
    select(doc_.scene.idOf(hit)); // nothing hit: deselect
}

void EditorApp::focusSelection() {
    const Entity sel = selected();
    if (sel.isNull()) return;
    // Back the camera off along its current direction until the selection
    // fills a comfortable part of the view.
    const Aabb box = pickBounds(doc_.scene, *assets_, sel);
    const float radius = std::max(glm::length(box.halfExtents()), 0.25f);
    camera_.position = box.center() - camera_.forward() * (radius * 2.5f + 0.5f);
}

Vec3 EditorApp::placementPoint(const Ray& ray) const {
    // On the ground (y = 0) where the mouse points, or 5 m out if it points at the sky.
    if (const auto t = intersectPlane(ray, Vec3(0.0f), Math::WorldUp); t && *t < 200.0f) return ray.at(*t);
    return ray.at(5.0f);
}

void EditorApp::applyWorldMatrix(Entity e, const Mat4& world) {
    // The gizmo works in world space; the Transform is relative to the parent.
    const Entity parent = doc_.scene.parentOf(e);
    const Mat4 parentWorld = parent.isNull() ? Mat4(1.0f) : doc_.scene.worldMatrix(parent);
    Transform& t = doc_.scene.get<Transform>(e);
    Math::decomposeTRS(glm::inverse(parentWorld) * world, t.position, t.rotation, t.scale);
    t.resetInterpolation();
}

} // namespace Engine
