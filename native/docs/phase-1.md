# Phase 1: Window, input, main loop, logging, math

**Status: done.** It builds, the tests pass, and the sandbox runs (checked on Linux: GCC 13 Debug and Clang Release builds with warnings as errors, and a GCC build under AddressSanitizer and UndefinedBehaviorSanitizer).

> **Since Phase 2:** this demo is the `timestep_demo` program (`src/sandbox/timestep_demo.cpp`), and screenshots are PNG files. The name `sandbox` now belongs to the Phase 2 3D scene. The commands below use the new names.

## Plan

**Goal:** a program that opens a window with an OpenGL 4.1 context, reads the keyboard and mouse, and runs a main loop with a fixed-rate simulation and a free-running render rate, plus logging and a math library ready for the renderer.

| Piece | File | What it does |
|---|---|---|
| Build | `CMakeLists.txt`, `cmake/` | C++17, warnings on, fetches GLFW, GLM and doctest, builds glad |
| Logging | `core/Log.h/.cpp` | `ENGINE_INFO("Loaded ", n, " files")`: levels, timestamps, file and line, colors, a log file, test sinks |
| Asserts | `core/Assert.h` | `ENGINE_ASSERT(cond, "why")` logs and stops in debug builds |
| Math | `core/Math.h/.cpp` | Names for GLM types (`Vec3`, `Mat4`, `Quat`) and the helpers the engine needs, with our axis conventions written down |
| Time | `core/Time.h/.cpp` | `Clock` (real time) and `FixedTimestep` (the accumulator that decides how many fixed steps to run) |
| Input | `core/KeyCodes.h`, `core/Input.h/.cpp` | Key and mouse state with "pressed / released this frame", independent of GLFW so it can be unit tested |
| Window | `core/Window.h/.cpp` | RAII owner of the GLFW window and GL context; forwards OS events as callbacks |
| Loop | `core/Application.h/.cpp` | Runs fixed updates, the variable update and rendering every frame; frame statistics |
| Screenshots | `core/Screenshot.h/.cpp` | Saves the framebuffer to a `.ppm` image (F12); also lets an automated run check the output |
| Sandbox | `sandbox/main.cpp` (now `timestep_demo.cpp`) | A demo that makes fixed vs variable timesteps visible |
| Tests | `tests/` | Math, timestep, input and log tests that run without a window |

## Files added

```
native/
├── ARCHITECTURE.md
├── CMakeLists.txt                 the engine library, sandbox and tests
├── cmake/
│   ├── CompilerWarnings.cmake     xe_set_warnings(): /W4 or -Wall -Wextra ...
│   └── Dependencies.cmake         GLFW 3.4, GLM 1.0.1, doctest 2.4.11 (git, pinned)
├── docs/phase-1.md                this file
├── src/
│   ├── core/
│   │   ├── Application.h/.cpp     main loop, frame stats
│   │   ├── Assert.h               ENGINE_ASSERT
│   │   ├── Input.h/.cpp           key / mouse state and edges
│   │   ├── KeyCodes.h             Key, MouseButton
│   │   ├── Log.h/.cpp             ENGINE_TRACE/INFO/WARN/ERROR
│   │   ├── Math.h/.cpp            Vec3, Mat4, Quat, composeTRS, perspective...
│   │   ├── Screenshot.h/.cpp      framebuffer -> .ppm
│   │   ├── Time.h/.cpp            Clock, FixedTimestep
│   │   └── Window.h/.cpp          GLFW window + OpenGL 4.1 context
│   └── sandbox/
│       ├── CMakeLists.txt
│       └── main.cpp               the Phase 1 demo
├── tests/
│   ├── CMakeLists.txt             one ctest entry per suite
│   ├── test_main.cpp              doctest's main()
│   ├── input_tests.cpp
│   ├── log_tests.cpp
│   ├── math_tests.cpp
│   └── time_tests.cpp
└── third_party/glad/              generated OpenGL 4.1 core loader (checked in)
```

## Build and run

The first configure downloads GLFW, GLM and doctest with **git**, so git and an internet connection are needed once. After that, builds work offline.

### Windows

1. Install **Visual Studio 2022** with the *Desktop development with C++* workload (it includes CMake), and **Git**.
2. Open *Developer PowerShell for VS 2022* and run:

```powershell
cd xordor\native
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug
.\build\bin\Debug\timestep_demo.exe
```

Visual Studio can also open the `native` folder directly (*File > Open > Folder*) and builds it with its CMake support.

### macOS

```bash
xcode-select --install        # Apple's compilers, once
brew install cmake            # or download CMake from cmake.org
cd xordor/native
cmake -S . -B build
cmake --build build
ctest --test-dir build
./build/bin/timestep_demo
```

macOS supports OpenGL up to 4.1, which is exactly what the engine asks for. On a Retina screen the framebuffer is twice the window size; the log shows both.

### Linux

```bash
# Debian / Ubuntu
sudo apt install build-essential cmake git xorg-dev libgl-dev
# Fedora
sudo dnf install gcc-c++ cmake git libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel mesa-libGL-devel

cd xordor/native
cmake -S . -B build
cmake --build build -j
ctest --test-dir build
./build/bin/timestep_demo
```

Wayland desktops run it through XWayland. For native Wayland, install the Wayland dev packages and configure with `-DXE_GLFW_WAYLAND=ON`.

### Timestep demo controls and options

| Key | Does |
|---|---|
| A / D or arrows | Move the blue player (in the fixed update) |
| Space, W or Up | Jump (the press is caught per frame and used by the next fixed step) |
| 1 / 2 / 3 / 4 | Fixed update at 10 / 30 / 60 / 120 Hz |
| I | Interpolation on / off (try it at 10 Hz) |
| V | Vsync on / off |
| Click / scroll | Move the ball to the mouse / resize it |
| F12 | Save `screenshot.png` |
| H, Esc | Help, quit |

Command line: `timestep_demo --frames 600 --fixed-hz 30 --no-vsync --screenshot out.png` runs 600 frames and saves the last one. The log is also written to `timestep_demo.log`.

## How it works

### 1. One frame

```
 frame N ─────────────────────────────────────────────────────────────────►
 │ tick clock │ pollEvents │ fixed x0..N │ onUpdate │ onRender(α) │ swap │ endFrame │
       │            │            │             │            │                  │
   real time    OS events    gameplay at     cameras,    draw, blending    forget this
   since last   -> Input     exactly 1/60 s  UI, follows previous and      frame's presses
   frame                     each            real time   current state     and mouse delta
```

`Application::run()` is that loop and nothing else. A game overrides the three hooks.

### 2. Fixed timestep with an accumulator

Real frames are uneven; the simulation must not be. Each frame adds its real duration to an accumulator, and every whole step in it becomes one `onFixedUpdate(1/60)`:

```
 real frames:   |──── 25 ms ────|─ 8 ms ─|────── 40 ms ──────|
 accumulator:     25 → 1 step, 8.3 left
                                 16.3 → 0 steps (not enough yet)
                                          56.3 → 3 steps, 6.3 left
 fixed steps:   |16.7|          |         |16.7|16.7|16.7|
```

What is left over is less than one step. `alpha = leftover / step` says how far we are toward the next step, so rendering draws `lerp(previous, current, alpha)`. Run the sandbox at 10 Hz with interpolation off and the ball jumps; with it on, the ball is smooth while the simulation still runs only 10 times a second.

Two safety rails:
- **Frame cap (0.25 s):** a breakpoint or a dragged window does not ask for hundreds of catch-up steps.
- **Step cap (8 per frame):** if one step costs more time than it simulates, each frame would need more steps than the last (the "spiral of death"). Past the cap, the extra time is dropped and the game runs in slow motion instead of freezing. The count is logged when the app exits.

**A real bug the tests caught:** `0.01` cannot be stored exactly in binary. After four subtractions, "five steps of time" left `0.0099999…` in the accumulator, and a strict `>=` ran only four steps. `FixedTimestep::advance` now counts a step when it is within a millionth of a step of whole. The test "rounding error never loses a step" guards it.

### 3. Input edges and the fixed update

`Input` records *pressed this frame* from the events themselves, so a key tapped and released within one frame still counts. Edges last exactly one frame, and that collides with the fixed update:

```
 frame:          1          2          3
 fixed steps:    2          0          1
 Space pressed:  ── in frame 2 ──
 wasKeyPressed in onFixedUpdate:  frame 2 has no step → the jump is lost
 wasKeyPressed in onUpdate:       seen once, remembered, used by frame 3's step
```

So the rule is: **read edges in `onUpdate`, queue the intent, and let the next fixed step consume it.** Held keys (`isKeyDown`) are fine to read in the fixed update. The sandbox's `jumpQueued_` is this pattern; Phase 6 turns it into a general action buffer.

### 4. Ownership with RAII

```
 Sandbox (on the stack in main)
  └── Application
       ├── unique_ptr<Window>
       │     ├── shared_ptr<GlfwLibrary>   glfwInit() / glfwTerminate(), counted
       │     └── unique_ptr<GLFWwindow, GlfwWindowDeleter>   glfwDestroyWindow()
       └── Input, FixedTimestep, Clock, FrameStats   (plain values)
```

Nothing is closed by hand. When `main` returns, destructors run in reverse order: the window closes first, and only then does GLFW shut down (that is why `glfw_` is declared before `handle_` in `Window`). If the constructor throws halfway, the members already built are still destroyed, so nothing leaks even on errors. `main` catches the exception and prints a readable message.

### 5. Keeping GLFW behind one file

`Window.h` only forward-declares `struct GLFWwindow;`, and CMake links GLFW `PRIVATE`. The sandbox, the tests and every later module can't see GLFW at all. Input events reach `Input` through plain `std::function` callbacks that `Application` wires up, so `Input` is tested with no window, and replacing GLFW would touch one file.

Key codes use GLFW's numbers so conversion is a cast, and `static_assert`s in `Window.cpp` stop the build if they ever disagree.

### 6. Logging

`ENGINE_INFO(a, b, c)` expands to `Log::write(level, __FILE__, __LINE__, a, b, c)`. The level is checked first, so a disabled message never formats anything (a test proves it). A C++17 fold expression `(stream << ... << args)` streams any type. Logger state lives in a function-local static so it exists before any global could use it. A mutex keeps lines from different threads from mixing, and the minimum level is atomic because it is read without the lock.

### 7. OpenGL context and glad

The OS only exports OpenGL 1.1 functions directly. Everything newer (shaders, buffers) must be looked up by name after a context exists. `gladLoadGL(glfwGetProcAddress)` does those lookups once. We request a **4.1 core** context (the newest macOS has; "core" removes the old fixed-function API). A driver may give a newer compatible version, as Mesa did here (4.5); the engine still only uses 4.1 features.

## Exercises

1. **Slow motion.** Multiply the time passed to `FixedTimestep::advance` by 0.25 while Shift is held. The ball should move in slow motion while the window stays responsive. What happens to `alpha`, and why does interpolation keep slow motion smooth?
2. **Frame time graph.** Keep the last 120 frame times in a ring buffer and draw them as thin bars along the top with `fillRect`. Compare vsync on and off, and watch what dragging the window does.
3. **Deterministic replay.** Record the keys held during each fixed step, then replay them in a second run and check the player ends at exactly the same position. Then try it with the frame time fed straight into the simulation instead of fixed steps, and see it drift.

## Known limitations and what comes next

- **Drawing is scissor-and-clear only.** Phase 2 adds shaders, meshes, textures, a camera and a forward renderer. *(Done in Phase 2.)*
- **No OpenGL error reporting.** Debug output (`KHR_debug`) is core only from 4.3. Phase 2 adds a `glGetError` check helper for debug builds. *(Done in Phase 2: `XE_GL_CHECK`.)*
- **Previous state is stored by hand** for interpolation (`Body::previous`). Phase 3's `Transform` component keeps the previous transform automatically.
- **Input is basic.** No gamepads, text input, key rebinding or action mapping yet (Phase 6). Edges must be read in `onUpdate`, as described above.
- **Screenshots are `.ppm`.** Phase 2 brings in stb and can write `.png`. *(Done in Phase 2.)*
- **Console colors are off on Windows** (marked `TODO` in `Log.cpp`: enabling virtual terminal processing).
- **Tested on Linux only so far.** The Windows and macOS steps above follow each tool's standard workflow but were not run here; please report anything that fails.
- doctest's CMake file prints a harmless "compatibility with CMake < 3.5" deprecation warning with CMake 3.27 and newer.

**Next: [Phase 2, renderer basics](phase-2.md).** RAII wrappers for shaders, vertex buffers and textures; a perspective camera you can fly with the mouse; and a forward renderer that draws lit, textured cubes with one directional light.
