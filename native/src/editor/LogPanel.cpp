// The log panel: recent ENGINE_INFO / WARN / ERROR messages inside the editor,
// collected by a Log sink (see EditorApp::onStart). Warnings and errors that
// would otherwise only reach the terminal, such as a missing texture or a
// shader that didn't compile, show up where you're looking.

#include "editor/EditorApp.h"

#include <imgui.h>

namespace Engine {

void EditorApp::drawLog() {
    if (!ImGui::Begin("Log")) {
        ImGui::End();
        return;
    }
    if (ImGui::Button("Clear")) logLines_.clear();
    ImGui::SameLine();
    ImGui::Checkbox("Follow", &logAutoScroll_);
    ImGui::Separator();
    ImGui::BeginChild("lines", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
    for (const LogLine& line : logLines_) {
        const ImVec4 color = line.level == LogLevel::Error ? ImVec4(1.0f, 0.45f, 0.4f, 1.0f)
                             : line.level == LogLevel::Warn ? ImVec4(1.0f, 0.8f, 0.35f, 1.0f)
                                                            : ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
        ImGui::TextColored(color, "%s", line.text.c_str());
    }
    if (logAutoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
    ImGui::End();
}

} // namespace Engine
