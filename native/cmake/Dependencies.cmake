# Third-party libraries, downloaded with git at configure time and pinned to
# exact release tags so every build uses the same code.
#
# FetchContent builds each library as part of our own build, so there is
# nothing to install by hand, and the same steps work on Windows, macOS and Linux.

include(FetchContent)

# Show download progress: the first configure clones a few repositories.
set(FETCHCONTENT_QUIET OFF)

# ---------------------------------------------------------------- GLFW: window, context, input
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
# X11 works on every Linux desktop (Wayland desktops run X11 apps through XWayland).
# Native Wayland needs extra dev packages, so it is opt-in.
set(GLFW_BUILD_WAYLAND ${XE_GLFW_WAYLAND} CACHE BOOL "" FORCE)
FetchContent_Declare(glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        3.4
  GIT_SHALLOW    TRUE
)

# ---------------------------------------------------------------- GLM: math
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        1.0.1
  GIT_SHALLOW    TRUE
)

# ---------------------------------------------------------------- stb: image loading and writing
# Single-file, public-domain libraries. stb has no release tags, so we pin a
# commit. It also has no CMake file: we only download it and make our own
# INTERFACE target that adds the include folder. The implementation is
# compiled once, in render/StbImpl.cpp.
FetchContent_Declare(stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG        db69a229ae5d89caff1d96b1af0c2a0fe0041c2b  # 2026-10-19: stb_image 2.30, stb_image_write 1.16
)

# ---------------------------------------------------------------- nlohmann/json: scene and .meta files
# Header-only. Chosen for readable code: j["position"] = {1, 2, 3} reads like
# the JSON it writes. (Slower than some parsers, which doesn't matter for
# files this size.)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
FetchContent_Declare(nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
  GIT_SHALLOW    TRUE
)

# ---------------------------------------------------------------- cgltf: glTF model files
# One header, like stb, with no CMake file; compiled once in assets/CgltfImpl.cpp.
FetchContent_Declare(cgltf
  GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
  GIT_TAG        v1.14
  GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(glfw glm stb nlohmann_json cgltf)

add_library(stb INTERFACE)
# SYSTEM: warnings inside stb's headers are not ours to fix.
target_include_directories(stb SYSTEM INTERFACE ${stb_SOURCE_DIR})
add_library(cgltf INTERFACE)
target_include_directories(cgltf SYSTEM INTERFACE ${cgltf_SOURCE_DIR})

if(XE_BUILD_EDITOR)
# ---------------------------------------------------------------- Dear ImGui: the editor's UI
# The "docking" branch adds dockable, rearrangeable panels. ImGui has no CMake
# file, so we build the core files and the two backends we need (GLFW for
# input, OpenGL 3 for drawing) into our own static library.
FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        v1.91.5-docking
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)
add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp     # ImGui::ShowDemoWindow, the best ImGui reference there is
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw)
# The editor loads a nicer font than ImGui's built-in pixel font, from here.
set(XE_IMGUI_FONT_DIR ${imgui_SOURCE_DIR}/misc/fonts CACHE INTERNAL "")
endif()

# ---------------------------------------------------------------- doctest: unit tests
if(XE_BUILD_TESTS)
  set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
  FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.11
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(doctest)
endif()
