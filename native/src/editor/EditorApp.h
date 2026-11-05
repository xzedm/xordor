#pragma once
// EditorApp: the Xordor editor.
//
//   ┌────────────────────────── menu bar ──────────────────────────┐
//   │ Outliner   │            Viewport                │ Inspector  │
//   │ (the tree) │  the scene, drawn into a texture,  │ (fields of │
//   │            │  with the gizmo on top             │  the       │
//   │            │                                    │  selected  │
//   ├────────────┴────────────────────────────────────┤  entity)   │
//   │ Content (asset files)  |  Log                   │            │
//   └─────────────────────────────────────────────────┴────────────┘
//
// The panels are docked windows (Dear ImGui's docking branch): drag a tab to
// rearrange them; the layout is remembered in editor_layout.ini.
//
// Dear ImGui is an "immediate mode" UI: there are no widget objects to keep
// in sync. Every frame we call ImGui::Button("Save") and so on again, and it
// returns true on the frame it was clicked. The UI is just a function of the
// current state, which makes an editor much simpler to write.
//
// Each panel is one member function, in a file of its own:
//   EditorApp.cpp        setup, the frame, menus, shortcuts, files
//   ViewportPanel.cpp    scene view, camera, picking, gizmo, drag-and-drop
//   OutlinerPanel.cpp    hierarchy tree, drag to reparent
//   InspectorPanel.cpp   component fields, generated from JSON + FieldHints
//   ContentPanel.cpp     asset browser
//   LogPanel.cpp         log messages
//
// Every change to the scene goes through the UndoStack (editor/Commands.h).

#include "assets/AssetManager.h"
#include "assets/AssetRegistry.h"
#include "assets/SceneSerializer.h"
#include "core/Application.h"
#include "core/Log.h"
#include "editor/Commands.h"
#include "editor/Gizmo.h"
#include "render/Camera.h"
#include "render/FlyCameraController.h"
#include "render/ForwardRenderer.h"
#include "render/Framebuffer.h"
#include "scene/SceneRenderer.h"

#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Engine {

struct EditorOptions {
    std::string scenePath = "scenes/sandbox.scene.json"; // relative to the assets folder
    long long frames = 0;         // stop after this many frames (automated checks)
    std::string screenshotPath;   // saved on the last frame of a --frames run
    std::string select;           // select the entity with this name at start
    std::string gizmo = "move";   // start in move / rotate / scale mode
};

class EditorApp final : public Application {
public:
    EditorApp(ApplicationSettings settings, EditorOptions options);
    ~EditorApp() override;

protected:
    void onStart() override;
    void onUpdate(double frameSeconds) override;
    void onRender(double alpha) override;
    void onShutdown() override;
    void onFramebufferResize(int width, int height) override;

private:
    // ---------------------------------------------------------------- setup (EditorApp.cpp)
    void initImGui();
    void shutdownImGui();

    // ---------------------------------------------------------------- the frame
    void drawDockSpace();
    void buildDefaultLayout(unsigned int dockspaceId);
    void drawMenuBar();
    void drawCreateMenuItems(SceneId parent);
    void handleShortcuts();
    void drawUnsavedChangesPopup();
    void drawSaveAsPopup();
    void runDeferred();

    // ---------------------------------------------------------------- panels
    void drawViewport();      // ViewportPanel.cpp
    void drawOutliner();      // OutlinerPanel.cpp
    void drawInspector();     // InspectorPanel.cpp
    void drawContent();       // ContentPanel.cpp
    void drawLog();           // LogPanel.cpp

    // Viewport helpers
    void updateEditorCamera(float dt);
    void pickAt(const Vec2& mouse, const Vec2& viewportPos, const Vec2& viewportSize);
    void focusSelection();
    Vec3 placementPoint(const Ray& ray) const; // where a dropped asset lands
    void applyWorldMatrix(Entity e, const Mat4& world);
    CameraView editorView(float aspect) const;

    // Outliner helpers
    void drawOutlinerNode(Entity e);

    // Inspector helpers (generated UI)
    void drawComponent(Entity e, const std::string& key);
    bool editField(const std::string& componentKey, const std::string& path, const std::string& label, Json& value);
    bool assetSlot(const std::string& label, Json& value, AssetType type);
    std::string assetLabel(AssetId id) const;

    // ---------------------------------------------------------------- document
    void newScene();
    bool openScene(const std::string& relativePath);
    bool saveScene();                 // asks for a name if there's none yet
    bool saveSceneAs(const std::string& relativePath);
    // Runs `action` now, or after asking about unsaved changes.
    void whenChangesHandled(std::function<void()> action);

    void execute(std::unique_ptr<Command> command);
    void undo();
    void redo();
    Entity selected() const { return doc_.scene.findById(doc_.selected); }
    void select(SceneId id) { doc_.selected = id; }
    void createEntity(const std::string& name, SceneId parent, Json components);
    void deleteSelected();
    void duplicateSelected();
    void updateTitle();

    // Live edits (dragging a slider or the gizmo change the scene every frame):
    // remember the component as it was when the edit began, and push one
    // command when it ends. See InspectorPanel.cpp.
    void noteEdit(Entity e, const std::string& key, const Json& before);
    void commitEdit(bool force);

    // ---------------------------------------------------------------- state
    EditorOptions options_;
    AssetRegistry registry_;
    SceneSerializer serializer_;
    EditorDocument doc_;
    UndoStack undo_;
    std::string scenePath_; // relative to the assets folder; empty = never saved

    // GPU objects (destroyed before the window: see the Application base class).
    std::unique_ptr<ForwardRenderer> renderer_;
    std::unique_ptr<SceneRenderer> sceneRenderer_;
    std::unique_ptr<AssetManager> assets_;
    std::unique_ptr<Framebuffer> viewportTarget_;

    // Viewport
    Camera camera_;
    FlyCameraController flyController_;
    bool flying_ = false;
    Gizmo gizmo_;
    GizmoMode gizmoMode_ = GizmoMode::Translate;
    GizmoSpace gizmoSpace_ = GizmoSpace::World;
    GizmoSnap snap_;
    bool viewportHovered_ = false;
    float frameSeconds_ = 0.0f;

    struct PendingEdit {
        bool active = false;
        SceneId entity = 0;
        std::string key;
        Json before;
    } pending_;

    // Changing the scene while ImGui is walking it (say, deleting a node in the
    // middle of drawing the tree) would pull the data out from under the loop.
    // UI code queues such actions here; they run once the panels are drawn.
    std::vector<std::function<void()>> deferred_;

    std::function<void()> afterUnsavedPrompt_;
    bool openUnsavedPrompt_ = false;
    bool openSaveAs_ = false;
    bool quitConfirmed_ = false;
    bool showDemoWindow_ = false;
    char renameBuffer_[256] = {};
    SceneId renameBufferFor_ = 0;
    char saveAsBuffer_[128] = {};

    // The log panel's copy of recent messages.
    struct LogLine {
        LogLevel level;
        std::string text;
    };
    std::deque<LogLine> logLines_;
    int logSink_ = -1;
    bool logAutoScroll_ = true;
    bool layoutFileExisted_ = false;
    bool layoutChecked_ = false;
    bool resetLayout_ = false;
    std::string lastTitle_;
    bool imguiReady_ = false;
};

} // namespace Engine
