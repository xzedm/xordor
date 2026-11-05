# Xordor Engine: Architecture

A small, Unreal-style 3D engine written to be read. Every system is the simplest version that works, so you can see the whole idea before reaching for the clever one.

## Goals and ground rules

- **Learn by reading.** Small functions, plain names, comments that explain *why*.
- **One thread, one frame loop.** No job system until something is measurably slow.
- **Engine code never knows about the game.** Games and the editor are clients of the engine, never the other way round.
- **RAII everywhere.** Every OS or GPU resource (window, buffer, texture, shader) is owned by exactly one C++ object that releases it in its destructor. No raw `new` / `delete`.
- **Conventions:** C++17, namespace `Engine::`, right-handed coordinates, **+Y up, -Z forward** (same as OpenGL and glTF), meters, radians in code (degrees only in the editor UI).

## Module layout

Each folder under `src/` is a module. A module may only depend on the modules above it in this list.

```
core/       Log, Assert, Math (GLM wrapper), Geometry, Time, Input, Window, App (Phases 1-3)
render/     Shader, Mesh, Texture, Camera, ForwardRenderer                        (Phase 2)
scene/      Entity, Registry, Scene, components, SceneRenderer, collision         (Phases 3 and 6)
assets/     AssetRegistry (.meta GUIDs), AssetManager caches, OBJ/glTF import, JSON scenes (Phase 4)
scripting/  Lua state, bindings, ScriptComponent                                  (Phase 6)
editor/     Editor app: docking, viewport, outliner, inspector, undo/redo         (Phase 5)
game/       The sample shooter, built only on the public engine API               (Phase 7)
sandbox/    A playground app that grows with each phase
```

```
            game/      editor/      sandbox/        <- applications
               \          |           /
                \     scripting/     /
                 \        |         /
                  ----- assets/ ----
                          |
                        scene/
                          |
                        render/
                          |
                        core/  ---> GLFW, glad, GLM (third-party)
```

## Main classes

| Module | Class | Responsibility |
|---|---|---|
| core | `Log` | Leveled, timestamped messages to the console, a file and test sinks |
| core | `Window` | Owns the GLFW window and the OpenGL 4.1 context; turns OS events into callbacks |
| core | `Input` | Current key and mouse state plus "pressed / released this frame" edges |
| core | `FixedTimestep` | Decides how many fixed simulation steps to run for the real time that passed |
| core | `Application` | The main loop. Games subclass it and override `onFixedUpdate`, `onUpdate`, `onRender` |
| render | `Shader`, `Mesh`, `Texture` | Thin RAII wrappers around OpenGL objects |
| render | `ForwardRenderer` | Draws a list of meshes with one directional light |
| scene | `Entity`, `Registry`, `Scene` | An entity is a generational id. The registry keeps components in per-type packed arrays; the scene adds the parent-child hierarchy |
| scene | `Transform`, `MeshRenderer`, `CameraComponent`, `LightComponent`, `Collider` | Plain data structs, no virtual functions |
| scene | `SceneRenderer` | The only bridge between scene and renderer: turns entities into draw calls |
| assets | `AssetRegistry`, `AssetManager` | The registry maps GUIDs (kept in `.meta` files) to files; the manager loads each asset once and shares it. Scenes refer to assets by id, never by path |
| assets | `SceneSerializer` | Scenes to and from readable JSON; apps register their own components |
| scripting | `ScriptHost` | Owns the Lua state and calls `onStart`, `onUpdate`, `onCollision` on scripts |
| editor | `EditorApp`, `UndoStack`, `Gizmo`, panels | Edits a `Scene` through commands that can be undone; the inspector is generated from component JSON |

## How one frame flows from input to pixels

```
 OS events (keyboard, mouse, resize)
      |  glfwPollEvents()
      v
 Window callbacks ---> Input (down state, pressed/released edges, mouse delta)
      |
      v
 Application::run, once per frame
   1. measure real time since last frame          (Clock)
   2. run 0..N fixed steps of exactly 1/60 s       (FixedTimestep)
        onFixedUpdate(dt): gameplay, physics, scripts   -> deterministic
   3. onUpdate(frameTime): camera, animation, editor UI -> smooth
   4. onRender(alpha): draw, blending the last two fixed states by alpha
        Scene -> visible MeshRenderers -> ForwardRenderer -> OpenGL
   5. swap buffers, then Input::endFrame() clears this frame's edges
```

Why two update rates: physics and gameplay behave the same on every machine only if they always step by the same `dt`. Rendering wants to run as often as the monitor allows. `alpha` (how far we are into the next fixed step) lets rendering interpolate between fixed states, so a 60 Hz simulation still looks smooth on a 144 Hz monitor.

## Data and files

- **Scenes** are JSON: a list of entities, each with its components and their fields. Readable in any text editor and friendly to version control.
- **Assets** (meshes, textures, scripts) are referenced by a 128-bit **GUID** stored in a small `.meta` file beside each source file, so renaming or moving a file never breaks a scene.
- **The editor** never changes the scene directly. Every change is a `Command` with `apply()` and `revert()`, which gives undo / redo for free.

## Scripting choice: Lua with sol2

We use **Lua 5.4 through sol2**:

- Lua is tiny (about 30 C files), starts in microseconds and is built for embedding. Defold, Roblox (Luau), Garry's Mod and many studios use it for exactly this job.
- sol2 binds C++ types to Lua in a few readable lines, with type checks and clear errors.
- Scripts reload while the game runs, so game logic changes without recompiling the engine.

A custom language would teach parsing, not engines, and would take longer than every other phase combined.

## Third-party libraries

| Library | Used for | Phase |
|---|---|---|
| GLFW 3.4 | Window, OpenGL context, input | 1 |
| glad 2 (generated, OpenGL 4.1 core) | Loading OpenGL functions | 1 |
| GLM 1.0.1 | Vectors, matrices, quaternions | 1 |
| doctest 2.4.11 | Unit tests | 1 |
| stb_image | Textures | 2 |
| cgltf (+ our own OBJ reader) | glTF / OBJ meshes. Chosen over Assimp: two formats don't justify a million-line dependency | 4 |
| nlohmann/json | Scene and asset files | 4 |
| Dear ImGui (docking) | Editor UI (gizmos are our own, on the engine's ray math) | 5 |
| Lua 5.4, sol2 | Scripting | 6 |
| miniaudio | Sound (optional) | later |

All are fetched by CMake at configure time, pinned to exact versions, except glad, which is generated once and checked in.

**Why OpenGL 4.1, not 4.5:** macOS stops at OpenGL 4.1. Using 4.1 everywhere keeps one code path for all three systems. The 4.5 features we give up (direct state access, debug output) are conveniences, not ideas you need in order to learn rendering.
