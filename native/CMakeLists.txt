# Xordor Engine: a small, readable C++ game engine.
#
#   cmake -S . -B build
#   cmake --build build
#
# The first configure downloads GLFW, GLM, stb and doctest with git (see cmake/Dependencies.cmake).

cmake_minimum_required(VERSION 3.20)
project(XordorEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Lets editors and clangd understand the code (writes build/compile_commands.json).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Single-config generators (Makefiles, Ninja) build Debug unless told otherwise,
# because asserts and readable stack traces matter more than speed while learning.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(XE_BUILD_TESTS "Build the unit tests" ON)
option(XE_BUILD_SANDBOX "Build the sandbox demo app" ON)
option(XE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(XE_GPU_TESTS "Also register the tests that open a hidden window and draw (need a GPU or a virtual display)" OFF)
option(XE_GLFW_WAYLAND "Linux: build GLFW with native Wayland support (needs Wayland dev packages)" OFF)

# Put every program in build/bin so they are easy to find on all platforms.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(cmake/CompilerWarnings.cmake)
include(cmake/Dependencies.cmake)

add_subdirectory(third_party/glad)

# ---------------------------------------------------------------- the engine
add_library(XordorEngine STATIC
  src/assets/AssetManager.cpp
  src/assets/AssetRegistry.cpp
  src/assets/CgltfImpl.cpp
  src/assets/GltfImport.cpp
  src/assets/JsonUtil.cpp
  src/assets/ObjImport.cpp
  src/assets/SceneSerializer.cpp
  src/core/Application.cpp
  src/core/AssetId.cpp
  src/core/Geometry.cpp
  src/core/Input.cpp
  src/core/Log.cpp
  src/core/Math.cpp
  src/core/Paths.cpp
  src/core/Screenshot.cpp
  src/core/Time.cpp
  src/core/Window.cpp
  src/render/Camera.cpp
  src/render/FlyCameraController.cpp
  src/render/ForwardRenderer.cpp
  src/render/GLCheck.cpp
  src/render/Image.cpp
  src/render/Mesh.cpp
  src/render/Primitives.cpp
  src/render/Shader.cpp
  src/render/ShaderLog.cpp
  src/render/StbImpl.cpp
  src/render/Texture.cpp
  src/scene/Collision.cpp
  src/scene/Registry.cpp
  src/scene/Scene.cpp
  src/scene/SceneRenderer.cpp
)
add_library(Xordor::Engine ALIAS XordorEngine)

# Public headers are included as "core/Log.h", "render/Shader.h", and so on.
target_include_directories(XordorEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# GLM and glad are PUBLIC because engine headers and apps use them directly.
# GLFW is PRIVATE: only Window.cpp talks to it, so apps never see GLFW types.
# stb and cgltf are PRIVATE too: Image.h and ModelImport.h wrap them.
# nlohmann_json is PUBLIC: asset headers use its Json type in their API.
target_link_libraries(XordorEngine
  PUBLIC glm::glm glad nlohmann_json::nlohmann_json
  PRIVATE glfw stb cgltf
)
target_compile_definitions(XordorEngine PUBLIC
  GLM_FORCE_XYZW_ONLY    # vec.x/.y/.z/.w only (no .r/.s aliases): clearer and faster to compile
  GLFW_INCLUDE_NONE      # glad provides the OpenGL declarations, not GLFW
  # Where shaders and textures live. Programs read them straight from the
  # source tree, so an edited shader is picked up without rebuilding. The
  # XORDOR_ASSETS environment variable overrides it (see core/Paths.h).
  XE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)
xe_set_warnings(XordorEngine)

# ---------------------------------------------------------------- apps and tests
if(XE_BUILD_SANDBOX)
  add_subdirectory(src/sandbox)
endif()

if(XE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
