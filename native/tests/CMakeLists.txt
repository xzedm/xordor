# Unit tests: plain functions and classes only, so they run without a window or GPU.
add_executable(engine_tests
  test_main.cpp
  input_tests.cpp
  log_tests.cpp
  math_tests.cpp
  time_tests.cpp
)
target_link_libraries(engine_tests PRIVATE Xordor::Engine doctest::doctest)
xe_set_warnings(engine_tests)

# Each test file shows up as its own entry in `ctest`, so a failure names the module.
foreach(suite input log math time)
  add_test(NAME ${suite} COMMAND engine_tests --test-suite=${suite})
endforeach()
