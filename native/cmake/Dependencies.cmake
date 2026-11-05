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

FetchContent_MakeAvailable(glfw glm)

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
