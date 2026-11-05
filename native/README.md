# Xordor Engine (C++)

A small, Unreal-style game engine written in C++17 to learn how engines work. Every system is the simplest version that works, comments explain *why*, and each phase ends with exercises.

- **Design:** [ARCHITECTURE.md](ARCHITECTURE.md) (modules, main classes, how a frame flows from input to pixels)
- **Stack:** OpenGL 4.1 (Windows, macOS, Linux), GLFW, glad, GLM, stb, doctest; later Dear ImGui, Assimp, Lua with sol2
- **Sample game (Phase 7):** a small arena shooter

## Phases

| Phase | Topic | Status |
|---|---|---|
| 1 | Window, input, main loop (fixed and variable timesteps), logging, math | [Done](docs/phase-1.md) |
| 2 | Renderer: shaders, meshes, textures, camera, forward renderer with a directional light | [Done](docs/phase-2.md) |
| 3 | Scene graph and entity-component system: Transform, MeshRenderer, Camera, Light, Collider | [Done](docs/phase-3.md) |
| 4 | Assets: GUIDs, OBJ / glTF import, texture cache, scenes saved as JSON | Next |
| 5 | Editor: docking, viewport with gizmos, outliner, inspector, undo / redo | |
| 6 | Gameplay: Lua scripting, player controller, collisions, events | |
| 7 | Sample game: a playable shooter level | |

## Quick start

```bash
cd native
cmake -S . -B build          # first run downloads GLFW, GLM, stb and doctest with git
cmake --build build
ctest --test-dir build       # unit tests (add -DXE_GPU_TESTS=ON to the first line for the GPU tests)
./build/bin/sandbox          # the entity scene (Windows: .\build\bin\Debug\sandbox.exe)
./build/bin/render_demo      # Phase 2: the renderer on its own
./build/bin/timestep_demo    # Phase 1: the main loop
```

Per-platform prerequisites are in [docs/phase-1.md](docs/phase-1.md#build-and-run); sandbox controls are in [docs/phase-3.md](docs/phase-3.md#sandbox-controls).

## Layout

```
src/core/      engine basics: window, input, main loop, logging, math (Phase 1)
src/render/    shaders, meshes, textures, camera, forward renderer (Phase 2)
src/scene/     entities, components, hierarchy, collision queries, scene renderer (Phase 3)
src/sandbox/   demo apps that grow each phase
assets/        shaders and textures, read at runtime
tests/         doctest unit tests and GPU tests
tools/         small scripts (texture generator)
cmake/         build helpers and pinned dependencies
third_party/   generated OpenGL loader (glad)
docs/          one write-up per phase
```

Later phases add `src/assets/`, `src/editor/`, `src/scripting/` and `src/game/`, in that dependency order.
