# The editor: a library (so the tests can use the commands, gizmo math and
# field hints without opening a window) plus the `editor` program.
add_library(XordorEditor STATIC
  Commands.cpp
  FieldHints.cpp
  Gizmo.cpp
  EditorApp.cpp
  ContentPanel.cpp
  InspectorPanel.cpp
  LogPanel.cpp
  OutlinerPanel.cpp
  ViewportPanel.cpp
)
add_library(Xordor::Editor ALIAS XordorEditor)
target_link_libraries(XordorEditor PUBLIC Xordor::Engine imgui)
# EditorApp hands the GLFW window to ImGui's backend.
target_link_libraries(XordorEditor PRIVATE glfw)
target_compile_definitions(XordorEditor PRIVATE XE_IMGUI_FONT_DIR="${XE_IMGUI_FONT_DIR}")
xe_set_warnings(XordorEditor)

add_executable(editor main.cpp)
target_link_libraries(editor PRIVATE Xordor::Editor)
xe_set_warnings(editor)
