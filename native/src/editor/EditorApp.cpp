#include "editor/EditorApp.h"

#include "core/Paths.h"
#include "core/Screenshot.h"

#include <glad/gl.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h> // DockBuilder: lays out the default panels in code

#include <cstdio>
#include <filesystem>

namespace Engine {
namespace {

const char* LayoutFile = "editor_layout.ini";

} // namespace

EditorApp::EditorApp(ApplicationSettings settings, EditorOptions options)
    : Application(std::move(settings)), options_(std::move(options)), registry_(assetDirectory()), serializer_(&registry_) {
    doc_.serializer = &serializer_;
}

EditorApp::~EditorApp() {
    if (logSink_ >= 0) {
        Log::removeSink(logSink_);
    }
}

// ---------------------------------------------------------------- setup

void EditorApp::onStart() {
    // The log panel keeps its own copy of recent messages.
    logSink_ = Log::addSink([this](const LogRecord& record) {
        logLines_.push_back({record.level, std::string(record.time) + "  " + std::string(record.message)});
        while (logLines_.size() > 1000) logLines_.pop_front();
    });

    renderer_ = std::make_unique<ForwardRenderer>();
    sceneRenderer_ = std::make_unique<SceneRenderer>(*renderer_);
    sceneRenderer_->showIcons = true;
    viewportTarget_ = std::make_unique<Framebuffer>(640, 360);
    registry_.scan();
    assets_ = std::make_unique<AssetManager>(registry_);
    initImGui();

    if (!openScene(options_.scenePath)) {
        newScene();
    }
    if (!options_.select.empty()) {
        select(doc_.scene.idOf(doc_.scene.findByName(options_.select)));
    }
    gizmoMode_ = options_.gizmo == "rotate" ? GizmoMode::Rotate : options_.gizmo == "scale" ? GizmoMode::Scale : GizmoMode::Translate;
    ENGINE_INFO("Editor ready. Right mouse + WASD flies; W / E / R pick move / rotate / scale; Ctrl+Z undoes.");
}

void EditorApp::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
    layoutFileExisted_ = std::filesystem::exists(LayoutFile);
    io.IniFilename = LayoutFile; // panel layout, in the folder the editor runs from

    // A proportional font reads much better than ImGui's built-in pixel font.
    const std::string font = std::string(XE_IMGUI_FONT_DIR) + "/Roboto-Medium.ttf";
    if (!std::filesystem::exists(font) || !io.Fonts->AddFontFromFileTTF(font.c_str(), 16.0f)) {
        ENGINE_WARN("Font ", font, " not found; using ImGui's built-in font");
        io.Fonts->AddFontDefault();
    }
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;

    // install_callbacks = true: the GLFW backend adds its own input callbacks
    // and calls the ones Window already set, so our Input still sees every
    // event (the fly camera uses it).
    ImGui_ImplGlfw_InitForOpenGL(window().glfwHandle(), true);
    ImGui_ImplOpenGL3_Init("#version 410"); // GLSL version for OpenGL 4.1
    imguiReady_ = true;
}

void EditorApp::shutdownImGui() {
    if (!imguiReady_) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    imguiReady_ = false;
}

void EditorApp::onShutdown() {
    shutdownImGui();
}

void EditorApp::onFramebufferResize(int, int) {
    // Nothing to do: the window is cleared and ImGui drawn at whatever size it
    // has each frame, and the viewport sizes its own framebuffer.
}

// ---------------------------------------------------------------- the frame

void EditorApp::onUpdate(double frameSeconds) {
    frameSeconds_ = static_cast<float>(frameSeconds);
    // The close button: ask about unsaved changes first.
    if (window().shouldClose() && undo_.isDirty() && !quitConfirmed_) {
        window().cancelClose();
        whenChangesHandled([this] {
            quitConfirmed_ = true;
            quit();
        });
    }
    renderer_->reloadShadersIfChanged(frameSeconds);
}

void EditorApp::onRender(double) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    drawMenuBar();
    drawDockSpace();
    drawViewport();
    drawOutliner();
    drawInspector();
    drawLog();
    drawContent(); // after Log: on first run, the last tab drawn starts in front
    if (showDemoWindow_) ImGui::ShowDemoWindow(&showDemoWindow_);
    drawUnsavedChangesPopup();
    drawSaveAsPopup();
    handleShortcuts();
    commitEdit(false);
    runDeferred();
    updateTitle();

    // The window itself: clear it, then let ImGui draw every panel. (The
    // scene was already drawn into the viewport's framebuffer.)
    ImGui::Render();
    const IVec2 fb = window().framebufferSize();
    Framebuffer::bindDefault();
    glViewport(0, 0, fb.x, fb.y);
    glClearColor(0.08f, 0.08f, 0.09f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (options_.frames > 0 && stats().frame + 1 == options_.frames && !options_.screenshotPath.empty()) {
        saveScreenshot(options_.screenshotPath, fb.x, fb.y);
    }
}

void EditorApp::runDeferred() {
    auto actions = std::move(deferred_);
    deferred_.clear();
    for (auto& action : actions) action();
}

void EditorApp::drawDockSpace() {
    const ImGuiID dockspace = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
    if ((!layoutChecked_ && !layoutFileExisted_) || resetLayout_) {
        buildDefaultLayout(dockspace);
    }
    layoutChecked_ = true;
    resetLayout_ = false;
}

void EditorApp::buildDefaultLayout(unsigned int dockspaceId) {
    // First run (no editor_layout.ini yet): split the space into the layout
    // drawn at the top of EditorApp.h. Afterwards ImGui remembers whatever
    // the user arranges.
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);
    ImGuiID center = dockspaceId;
    const ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.26f, nullptr, &center);
    const ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.22f, nullptr, &center);
    const ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.28f, nullptr, &center);
    ImGui::DockBuilderDockWindow("Viewport", center);
    ImGui::DockBuilderDockWindow("Outliner", left);
    ImGui::DockBuilderDockWindow("Inspector", right);
    ImGui::DockBuilderDockWindow("Log", bottom);
    ImGui::DockBuilderDockWindow("Content", bottom);
    ImGui::DockBuilderFinish(dockspaceId);
}

void EditorApp::drawCreateMenuItems(SceneId parent) {
    const auto mesh = [this](AssetId id) { return Json{{"meshRenderer", {{"mesh", serializer_.assetRef(id)}}}}; };
    if (ImGui::MenuItem("Empty")) createEntity("Empty", parent, Json::object());
    if (ImGui::MenuItem("Cube")) createEntity("Cube", parent, mesh(BuiltinAssets::Cube));
    if (ImGui::MenuItem("Sphere")) createEntity("Sphere", parent, mesh(BuiltinAssets::Sphere));
    if (ImGui::MenuItem("Plane")) createEntity("Plane", parent, mesh(BuiltinAssets::Plane));
    ImGui::Separator();
    if (ImGui::MenuItem("Camera")) createEntity("Camera", parent, Json{{"camera", {{"primary", false}}}});
    if (ImGui::MenuItem("Light")) createEntity("Light", parent, Json{{"light", Json::object()}});
}

void EditorApp::drawMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Scene", "Ctrl+N")) whenChangesHandled([this] { newScene(); });
        if (ImGui::BeginMenu("Open Scene")) {
            bool any = false;
            for (const AssetRecord* record : registry_.all()) {
                if (record->type != AssetType::Scene) continue;
                any = true;
                if (ImGui::MenuItem(record->path.c_str(), nullptr, record->path == scenePath_)) {
                    const std::string path = record->path;
                    whenChangesHandled([this, path] { openScene(path); });
                }
            }
            if (!any) ImGui::TextDisabled("No .scene.json files in the assets folder");
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Save", "Ctrl+S")) saveScene();
        if (ImGui::MenuItem("Save As...")) openSaveAs_ = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Quit")) whenChangesHandled([this] {
            quitConfirmed_ = true;
            quit();
        });
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        const std::string undoLabel = undo_.canUndo() ? "Undo " + undo_.undoName() : "Undo";
        const std::string redoLabel = undo_.canRedo() ? "Redo " + undo_.redoName() : "Redo";
        if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, undo_.canUndo())) undo();
        if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, undo_.canRedo())) redo();
        ImGui::Separator();
        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, !selected().isNull())) duplicateSelected();
        if (ImGui::MenuItem("Delete", "Del", false, !selected().isNull())) deleteSelected();
        if (ImGui::MenuItem("Focus", "F", false, !selected().isNull())) focusSelection();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Create")) {
        drawCreateMenuItems(0);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Colliders", nullptr, &sceneRenderer_->showColliders);
        ImGui::MenuItem("Icons for cameras, lights and empties", nullptr, &sceneRenderer_->showIcons);
        ImGui::MenuItem("Wireframe", nullptr, &renderer_->wireframe);
        ImGui::Separator();
        if (ImGui::MenuItem("Reload Shaders")) renderer_->reloadShaders();
        if (ImGui::MenuItem("Rescan Assets")) registry_.scan();
        if (ImGui::MenuItem("Reset Panel Layout")) resetLayout_ = true;
        ImGui::MenuItem("ImGui Demo (widget reference)", nullptr, &showDemoWindow_);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        ImGui::TextUnformatted("Viewport: hold right mouse + WASD / QE to fly, scroll for speed.");
        ImGui::TextUnformatted("Left click selects. W / E / R: move / rotate / scale. Hold Ctrl to snap.");
        ImGui::TextUnformatted("F focuses the selection. Drag assets from Content into the viewport.");
        ImGui::TextUnformatted("Outliner: drag an entity onto another to parent it.");
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

void EditorApp::handleShortcuts() {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput || flying_) {
        return; // typing in a text box, or WASD is flying the camera
    }
    const bool ctrl = io.KeyCtrl;
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) io.KeyShift ? redo() : undo();
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) redo();
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) saveScene();
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) duplicateSelected();
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_N, false)) whenChangesHandled([this] { newScene(); });
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) deleteSelected();
    if (!ctrl && viewportHovered_) {
        if (ImGui::IsKeyPressed(ImGuiKey_W, false)) gizmoMode_ = GizmoMode::Translate;
        if (ImGui::IsKeyPressed(ImGuiKey_E, false)) gizmoMode_ = GizmoMode::Rotate;
        if (ImGui::IsKeyPressed(ImGuiKey_R, false)) gizmoMode_ = GizmoMode::Scale;
        if (ImGui::IsKeyPressed(ImGuiKey_F, false)) focusSelection();
    }
}

// ---------------------------------------------------------------- popups

void EditorApp::whenChangesHandled(std::function<void()> action) {
    if (!undo_.isDirty()) {
        deferred_.push_back(std::move(action));
        return;
    }
    afterUnsavedPrompt_ = std::move(action);
    openUnsavedPrompt_ = true;
}

void EditorApp::drawUnsavedChangesPopup() {
    if (openUnsavedPrompt_) {
        ImGui::OpenPopup("Unsaved changes");
        openUnsavedPrompt_ = false;
    }
    if (!ImGui::BeginPopupModal("Unsaved changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    ImGui::Text("%s has unsaved changes.", scenePath_.empty() ? "The scene" : scenePath_.c_str());
    if (ImGui::Button("Save")) {
        if (saveScene()) {
            deferred_.push_back(std::move(afterUnsavedPrompt_));
        }
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Don't Save")) {
        deferred_.push_back(std::move(afterUnsavedPrompt_));
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        afterUnsavedPrompt_ = nullptr;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void EditorApp::drawSaveAsPopup() {
    if (openSaveAs_) {
        ImGui::OpenPopup("Save Scene As");
        std::snprintf(saveAsBuffer_, sizeof saveAsBuffer_, "%s", "new_scene");
        openSaveAs_ = false;
    }
    if (!ImGui::BeginPopupModal("Save Scene As", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    ImGui::TextUnformatted("Saved as assets/scenes/<name>.scene.json");
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    const bool enter = ImGui::InputText("Name", saveAsBuffer_, sizeof saveAsBuffer_, ImGuiInputTextFlags_EnterReturnsTrue);
    if ((ImGui::Button("Save") || enter) && saveAsBuffer_[0] != '\0') {
        saveSceneAs(std::string("scenes/") + saveAsBuffer_ + ".scene.json");
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

// ---------------------------------------------------------------- document

void EditorApp::newScene() {
    // Not empty: a camera, a sun and a floor, so there's something to see.
    Scene scene;
    const Entity cam = scene.createEntity("Main Camera");
    scene.add<CameraComponent>(cam);
    scene.get<Transform>(cam).position = Vec3(0.0f, 2.0f, 6.0f);
    const Entity sun = scene.createEntity("Sun");
    scene.add<LightComponent>(sun);
    scene.get<Transform>(sun).rotation = Math::quatFromEulerDegrees(Vec3(-50.0f, 35.0f, 0.0f));
    const Entity ground = scene.createEntity("Ground");
    scene.get<Transform>(ground).scale = Vec3(20.0f, 1.0f, 20.0f);
    MeshRenderer& mr = scene.add<MeshRenderer>(ground);
    mr.mesh = BuiltinAssets::Plane;
    mr.material.albedo = registry_.idForPath("textures/checker.png");
    mr.material.uvScale = Vec2(2.5f);
    doc_.scene = std::move(scene);
    doc_.selected = 0;
    scenePath_.clear();
    undo_.clear();
    camera_.position = Vec3(0.0f, 3.0f, 8.0f);
    camera_.lookAt(Vec3(0.0f));
    ENGINE_INFO("New scene");
}

bool EditorApp::openScene(const std::string& relativePath) {
    std::string error;
    Scene loaded;
    if (!serializer_.load(registry_.root() + "/" + relativePath, loaded, &error)) {
        ENGINE_ERROR("Could not open ", relativePath, ": ", error);
        return false;
    }
    doc_.scene = std::move(loaded);
    doc_.selected = 0;
    scenePath_ = relativePath;
    undo_.clear();
    // Start the editor camera where the scene's own camera is.
    const Entity cam = doc_.scene.primaryCamera();
    if (!cam.isNull()) {
        const Transform& t = doc_.scene.get<Transform>(cam);
        camera_.position = doc_.scene.worldPosition(cam);
        camera_.lookAt(camera_.position + t.rotation * Math::WorldForward);
    }
    ENGINE_INFO("Opened ", relativePath, " (", doc_.scene.entityCount(), " entities)");
    return true;
}

bool EditorApp::saveScene() {
    if (scenePath_.empty()) {
        openSaveAs_ = true;
        return false;
    }
    return saveSceneAs(scenePath_);
}

bool EditorApp::saveSceneAs(const std::string& relativePath) {
    commitEdit(true);
    const std::string path = registry_.root() + "/" + relativePath;
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    std::string error;
    if (!serializer_.save(doc_.scene, path, &error)) {
        ENGINE_ERROR("Save failed: ", error);
        return false;
    }
    registry_.registerFile(relativePath);
    scenePath_ = relativePath;
    undo_.markSaved();
    ENGINE_INFO("Saved ", relativePath);
    return true;
}

void EditorApp::execute(std::unique_ptr<Command> command) {
    commitEdit(true); // a live edit in progress becomes its own undo step first
    undo_.execute(std::move(command), doc_);
}

void EditorApp::undo() {
    commitEdit(true);
    if (undo_.canUndo()) ENGINE_INFO("Undo ", undo_.undoName());
    undo_.undo(doc_);
}

void EditorApp::redo() {
    commitEdit(true);
    if (undo_.canRedo()) ENGINE_INFO("Redo ", undo_.redoName());
    undo_.redo(doc_);
}

void EditorApp::createEntity(const std::string& name, SceneId parent, Json components) {
    // New things appear in front of the camera (if they have no parent to sit in).
    if (parent == 0 && !components.contains("transform")) {
        const Vec3 at = camera_.position + camera_.forward() * 5.0f;
        components["transform"] = {{"position", jsonArray(at)}};
    }
    deferred_.push_back([this, name, parent, components] { execute(makeCreateEntityCommand(doc_, name, parent, components)); });
}

void EditorApp::deleteSelected() {
    if (selected().isNull()) return;
    const SceneId id = doc_.selected;
    deferred_.push_back([this, id] {
        if (!doc_.scene.findById(id).isNull()) execute(std::make_unique<DeleteEntityCommand>(doc_, id));
    });
}

void EditorApp::duplicateSelected() {
    if (selected().isNull()) return;
    const SceneId id = doc_.selected;
    deferred_.push_back([this, id] {
        if (auto command = makeDuplicateCommand(doc_, id)) execute(std::move(command));
    });
}

void EditorApp::updateTitle() {
    const std::string title = std::string("Xordor Editor  |  ") + (scenePath_.empty() ? "(unsaved scene)" : scenePath_) +
                              (undo_.isDirty() ? " *" : "");
    if (title != lastTitle_) {
        window().setTitle(title);
        lastTitle_ = title;
    }
}

} // namespace Engine
