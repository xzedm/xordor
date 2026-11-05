# Xordor Engine (C++)

A small, Unreal-style game engine written in C++17 to learn how engines work. Every system is the simplest version that works, comments explain *why*, and each phase ends with exercises.

- **Design:** [ARCHITECTURE.md](ARCHITECTURE.md) (modules, main classes, how a frame flows from input to pixels)
- **Stack:** OpenGL 4.1 (Windows, macOS, Linux), GLFW, glad, GLM, doctest; later Dear ImGui, Assimp, stb, Lua with sol2
- **Sample game (Phase 7):** a small arena shooter

## Phases

| Phase | Topic | Status |
|---|---|---|
| 1 | Window, input, main loop (fixed and variable timesteps), logging, math | [Done](docs/phase-1.md) |
| 2 | Renderer: shaders, meshes, textures, camera, forward renderer with a directional light | Next |
| 3 | Scene graph and entity-component system: Transform, MeshRenderer, Camera, Light, Collider | |
| 4 | Assets: GUIDs, OBJ / glTF import, texture cache, scenes saved as JSON | |
| 5 | Editor: docking, viewport with gizmos, outliner, inspector, undo / redo | |
| 6 | Gameplay: Lua scripting, player controller, collisions, events | |
| 7 | Sample game: a playable shooter level | |

## Quick start

```bash
cd native
cmake -S . -B build          # first run downloads GLFW, GLM and doctest with git
cmake --build build
ctest --test-dir build       # unit tests
./build/bin/sandbox          # the Phase 1 demo (Windows: .\build\bin\Debug\sandbox.exe)
```

Per-platform prerequisites and controls are in [docs/phase-1.md](docs/phase-1.md#build-and-run).

## Layout

```
src/core/      engine basics (Phase 1)
src/sandbox/   demo app that grows each phase
tests/         doctest unit tests
cmake/         build helpers and pinned dependencies
third_party/   generated OpenGL loader (glad)
docs/          one write-up per phase
```

Later phases add `src/render/`, `src/scene/`, `src/assets/`, `src/editor/`, `src/scripting/` and `src/game/`, in that dependency order.
