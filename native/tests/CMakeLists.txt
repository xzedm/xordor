# Unit tests: plain functions and classes only, so they run without a window or GPU.
add_executable(engine_tests
  test_main.cpp
  asset_tests.cpp
  camera_tests.cpp
  ecs_tests.cpp
  geometry_tests.cpp
  image_tests.cpp
  input_tests.cpp
  log_tests.cpp
  math_tests.cpp
  model_tests.cpp
  primitives_tests.cpp
  scene_tests.cpp
  serializer_tests.cpp
  shader_log_tests.cpp
  time_tests.cpp
)
target_link_libraries(engine_tests PRIVATE Xordor::Engine doctest::doctest)
xe_set_warnings(engine_tests)

# Each test file shows up as its own entry in `ctest`, so a failure names the module.
foreach(suite assets camera ecs geometry image input log math models primitives scene serializer shaderlog time)
  add_test(NAME ${suite} COMMAND engine_tests --test-suite=${suite})
endforeach()

# GPU tests draw with the real renderer, so they need OpenGL 4.1 and a display.
# They are always built (so they keep compiling) but only registered with
# ctest when asked for, because build servers often have no GPU.
add_executable(gpu_tests test_main.cpp gpu_tests.cpp)
target_link_libraries(gpu_tests PRIVATE Xordor::Engine doctest::doctest)
xe_set_warnings(gpu_tests)
if(XE_GPU_TESTS)
  add_test(NAME gpu COMMAND gpu_tests)
endif()
