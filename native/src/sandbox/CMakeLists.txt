# Phase 2 sandbox: a small lit 3D scene (renderer, camera, textures, hot reload).
add_executable(sandbox main.cpp)
target_link_libraries(sandbox PRIVATE Xordor::Engine)
xe_set_warnings(sandbox)

# Phase 1 demo, kept because it shows the fixed and variable timesteps best.
add_executable(timestep_demo timestep_demo.cpp)
target_link_libraries(timestep_demo PRIVATE Xordor::Engine)
xe_set_warnings(timestep_demo)
