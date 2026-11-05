# Each phase's demo is kept, so the docs for every phase stay runnable.

# Phase 3 sandbox: a scene of entities and components (hierarchy, colliders).
add_executable(sandbox main.cpp)
target_link_libraries(sandbox PRIVATE Xordor::Engine)
xe_set_warnings(sandbox)

# Phase 2: the renderer used directly, without a scene.
add_executable(render_demo render_demo.cpp)
target_link_libraries(render_demo PRIVATE Xordor::Engine)
xe_set_warnings(render_demo)

# Phase 1: fixed and variable timesteps.
add_executable(timestep_demo timestep_demo.cpp)
target_link_libraries(timestep_demo PRIVATE Xordor::Engine)
xe_set_warnings(timestep_demo)
