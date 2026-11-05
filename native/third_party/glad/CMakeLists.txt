# glad: loads OpenGL 4.1 core functions at runtime.
#
# OpenGL functions beyond version 1.1 are not exported by the system library;
# their addresses must be asked for after a context exists. glad generates the
# code that does this. These files were generated once with:
#
#   pip install glad2
#   python -m glad --api gl:core=4.1 --out-path third_party/glad --reproducible c
#
# License: generated code is (WTFPL OR CC0-1.0) AND Apache-2.0 (Khronos headers).

add_library(glad STATIC src/gl.c)
target_include_directories(glad PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
