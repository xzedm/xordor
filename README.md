# Xordor

**A 3D game engine for your computer, made to be much easier than Unity or Unreal.**

Xordor is a desktop app for Windows, macOS and Linux. It has two ways to work on the same games:

| | **Simple mode** | **Pro mode** |
|---|---|---|
| Who it's for | Everyone: kids, teachers, artists, first-time game makers | Developers who know engines like Unreal or Unity |
| What you get | Big friendly buttons, plain words, click to add and drag to move, an AI helper that builds for you | An engine-style editor: Place Actors, Outliner, Details, Content Browser, Output Log, Code editor, levels, prefabs, multi-select, fly camera, snapping, wireframe and stats |

Switch at any time with the **Simple | Pro** switch in the top bar, or **View > Simple Mode / Pro Mode** (Ctrl+1 / Ctrl+2).

Both modes include an **AI helper** that works with **ChatGPT, Claude, Gemini, OpenRouter, DeepSeek, Mistral, Groq, Grok**, free local models (**Ollama**, **LM Studio**) or any OpenAI-compatible service. A free offline helper is built in.

---

## Install and run

You need [Node.js](https://nodejs.org) (the "LTS" version) once, to set Xordor up.

1. Download this project. On GitHub, press **Code > Download ZIP**, then unzip it. (Or `git clone` it.)
2. Open a terminal in the Xordor folder:
   - **Windows:** open the folder, click the address bar, type `cmd` and press Enter.
   - **macOS:** right-click the folder in Finder and choose **New Terminal at Folder**.
   - **Linux:** right-click the folder and choose **Open in Terminal**.
3. Type these two commands:

   ```bash
   npm install
   npm start
   ```

The first command downloads what Xordor needs (only the first time). The second opens the Xordor app. Next time, only `npm start` is needed.

The first time Xordor opens, it asks whether you want **Simple** or **Pro** mode, then lets you pick a starter game.

---

## Simple mode

| To do this | Do this |
|---|---|
| Add something | Click it in the **Add** panel on the left. It appears in the middle of your view. |
| Select something | Click it in the 3D view or in the **Objects** list. |
| Move, turn or resize | Drag the colored arrows. Switch tools with **Move**, **Turn** and **Size**, or press **W**, **E** or **R**. |
| Change color, material or pattern | Use the **Properties** panel on the right. |
| Make it do something | Under **Behaviors**, pick one, such as *Spin*, *Move back and forth*, *Collect for points*, *Hurts the player*, *Bounce pad* or *Finish line*. |
| Change the sky, ground and rules | Click empty space so nothing is selected. **Properties** then shows the world settings and game rules. |
| Add another level | Use the level menu in the top bar and choose **+ Add a level**. Winning a level moves on to the next one. |
| Look around | Drag with the mouse. Right-drag to slide. Scroll to zoom. |
| Undo | **Ctrl+Z** (redo is **Ctrl+Y**). Every AI change can also be undone from its reply. |

Press the green **Play** button to try your game: **W A S D** or the arrow keys move, **Space** jumps, **Shift** runs, drag the mouse to look, **P** pauses, **Esc** goes back to the editor.

---

## Pro mode

Pro mode lays the editor out the way big engines do:

- **Place Actors** (left): everything you can add, with search.
- **Viewport** (middle): a toolbar for move / rotate / scale, world or local axes, snapping with chosen step sizes, camera speed, **Lit / Unlit / Wireframe** views, the grid, and a **Stats** overlay (FPS, draw calls, triangles).
- **Outliner** (top right): every actor in the level, with folders, search, type column and visibility. Ctrl+click and Shift+click select several. Right-click for rename, duplicate, snap to floor, move to folder and save as prefab.
- **Details** (bottom right): exact transform, rendering, physics, components, folder, tags, the actor's ID, and **Edit as JSON**.
- **Bottom dock:**
  - **Content Browser**: your **levels** (open, rename, duplicate, reorder, choose where play starts), **your own prefabs**, the built-in library, and every script in the project.
  - **Output Log**: messages from your game, your scripts and the AI assistant, with filters. Script errors show the line number.
  - **Code**: a real code editor (syntax colors, auto-indent, error lines) for the **game script** and for **custom code components** on actors. **Reference** shows the whole scripting API.
  - **AI Assistant**: the same AI helper, docked.

**Camera like Unreal:** hold the **right mouse button** and use **W A S D** to fly, **Q / E** to go down / up, scroll to change speed. Left-drag orbits, middle-drag pans, **F** zooms to the selection, **End** drops it to the floor, **Space** cycles move / rotate / scale.

**Playing in Pro mode** keeps the panels open, like "Play in Editor". **Play From Camera** (Alt+Shift+P) starts the player where the camera is. The **Pause** button (or **P**) freezes the game.

### Scripting

Scripts are JavaScript. A **custom code** component on an actor can define:

```js
function onStart() {}        // once, when play starts
function onUpdate(dt) {}     // every frame
function onTouch() {}        // when the player touches this actor

// for example:
function onUpdate(dt) {
  self.rotation.y += dt;
  if (self.distanceToPlayer() < 3) game.message("Hello!");
}
```

The project's **game script** (Content Browser > Scripts) runs in every level and can define `onStart()` and `onUpdate(dt)`. Scripts can use `self`, `game`, `player`, `world`, `log()` and `THREE` (three.js). The full list is under **Code > Reference** and **Help > Script Reference**. The AI assistant can write scripts for you.

---

## The AI helper

Type what you want in plain words:

- *"Build an obstacle course with moving platforms and a finish flag"*
- *"Add 10 coins in a circle around the house"*
- *"Make it night and add some lights"*
- *"Write a script that makes this door open when I have 5 coins"* (select the door first)

Open **AI settings** (the gear in the AI panel, or **Edit > AI Settings**) and pick a service:

| Service | What you need | Where to get it |
|---|---|---|
| **Built-in helper** | Nothing. Free and offline, simple requests only. | Included |
| **ChatGPT (OpenAI)** | An OpenAI API key | [platform.openai.com/api-keys](https://platform.openai.com/api-keys) |
| **Claude (Anthropic)** | An Anthropic API key | [platform.claude.com/settings/keys](https://platform.claude.com/settings/keys) |
| **Gemini (Google)** | A Google AI Studio key | [aistudio.google.com/apikey](https://aistudio.google.com/apikey) |
| **OpenRouter** | One key for hundreds of models | [openrouter.ai/keys](https://openrouter.ai/keys) |
| **DeepSeek, Mistral, Groq, Grok** | That company's API key | Each company's developer site |
| **Ollama** (free, on your computer) | Install [Ollama](https://ollama.com/download) and download a model, for example `ollama pull llama3.2` | [ollama.com](https://ollama.com) |
| **LM Studio** (free, on your computer) | Start LM Studio's local server | [lmstudio.ai](https://lmstudio.ai) |
| **Other** | Any OpenAI-compatible `/chat/completions` address | Your provider |

Press **Load models** to see the models your account can use.

- An API key is not the same as a ChatGPT Plus or Claude Pro subscription. API use is billed per request by the company you pick; a typical request costs a fraction of a cent to a few cents.
- Keys are stored by the desktop app, encrypted with your system's key store (DPAPI on Windows, Keychain on macOS, the system keyring on Linux when one is available). The editor window never sees them.
- AI requests are sent by the desktop app itself, so services that block web pages still work, and local models need no extra setup.
- Each request sends a short description of the current level (object names, shapes, positions and rules).

---

## Projects and sharing

- **File > Save** (Ctrl+S) saves a `.xordor` project file wherever you like. It is plain JSON, so it works well with Git.
- **File > Open Recent** lists your recent projects. Xordor also keeps a recovery copy of your work in case it closes unexpectedly, and asks before closing with unsaved changes.
- **File > Export Playable Game** (Ctrl+E) makes one `.html` file with every level inside. Anyone can double-click it to play, with no install and no internet. You can upload it to game sites such as itch.io (as an HTML game). Your AI keys are never included.

---

## For developers

Xordor is plain JavaScript (no UI framework) with [three.js](https://threejs.org) for rendering, [cannon-es](https://github.com/pmndrs/cannon-es) for physics, [CodeMirror](https://codemirror.net) for the code editor and [Electron](https://www.electronjs.org) for the desktop app. It needs Node.js 20.19 or newer.

```bash
npm install     # install dependencies (downloads Electron)
npm start       # prepare the editor and open the desktop app
npm run dev     # open the desktop app with live reload while you change the code
npm test        # run the unit tests (Vitest)
```

### How it fits together

```
desktop/
  main.js          Electron main process: window, native menus, project files,
                   encrypted key storage, AI requests, unsaved-changes prompt
  preload.cjs      The only bridge between the editor window and the computer
scripts/dev.mjs    Starts Vite and Electron together for development
src/
  core/            Plain data, no 3D needed (unit tested)
    project.js       Project format: settings, levels, prefabs, game script, validation
    shapes.js        Every shape: friendly info plus how to build it
    prefabs.js       The Add / Place Actors palette
    behaviors.js     Behaviors (components): names, settings and defaults
    environment.js   Sky presets
    log.js           The shared log behind the Output Log
  engine/          The runtime
    world.js         Project data to Three.js scene (shared by editor and game)
    physics.js       cannon-es bodies built from each shape's parts
    Game.js          Play mode: player, camera, rules, levels, scripts, HUD
    behaviorRuntime.js  What each behavior does while playing
  editor/          The editor
    Editor.js        Project state, levels, undo / redo, multi-select, gizmos, fly camera
    panels.js        Simple mode panels (Add, Objects) and the shared Details / Properties panel
    proPanels.js     Pro mode panels (Outliner, Content Browser, Output Log, Code, viewport toolbar)
    aiPanel.js       AI chat and AI settings
    codeEditor.js    CodeMirror setup
    storage.js       Recovery copy, project files, game export
  ai/              The AI layer
    providers.js     OpenAI-compatible, Anthropic (official SDK) and Gemini clients
    prompt.js        System prompt (generated from the catalogs) and level snapshot
    parse.js         Forgiving reader for model replies
    actions.js       Applies AI actions to a level (pure, one undo step)
    offline.js       The rule-based built-in helper
  platform.js      Desktop app bridge used by the editor
  templates.js     Starter games, built with the same actions the AI uses
```

**Projects** hold game-wide settings, a game script, user prefabs and a list of levels. The editor, the AI and the game all work on a *level view* (the settings plus one level's sky and objects), and changes are merged back into the project as one undo step.

**The AI never touches the scene directly.** It returns JSON actions (`add`, `addMany`, `update`, `delete`, `environment`, `settings`, `gameScript`...). `applyActions()` validates them into a new level view, which the editor commits as a single undo step. The system prompt is generated from the shape, prefab and behavior catalogs, so adding a shape or behavior teaches the AI about it automatically.

**Adding a shape:** add it to `SHAPES` in `src/core/shapes.js` (a label, a base size and a `build` function returning mesh parts; mark solid parts with `collider`), and to `src/core/prefabs.js` for a palette button.

**Adding a behavior:** describe it in `src/core/behaviors.js` and implement it in `src/engine/behaviorRuntime.js`. Both modes build their settings forms from the description.

**Claude settings:** the Anthropic client uses the official `@anthropic-ai/sdk`, streams replies, sets `output_config.effort` (Fast / Balanced / Thorough in AI settings), caches the system prompt, and turns on server-side refusal fallbacks (`fallbacks: "default"`) for models that support them.

### Security

- The editor window runs with context isolation and no Node.js access. It can reach the computer only through `desktop/preload.cjs`.
- Project code (custom code components and the game script) runs in the editor window. To keep that safe, the desktop app only reads or writes files you picked in a dialog or opened from Recent, ignores address changes for built-in AI services, and only sends a custom server's key to the address it was saved for.
- Opening a project that contains code asks whether to keep or remove it.

---

## Credits

Built on open-source projects: [three.js](https://threejs.org) (MIT), [cannon-es](https://github.com/pmndrs/cannon-es) (MIT), [Electron](https://www.electronjs.org) (MIT), [CodeMirror](https://codemirror.net) (MIT), the [Anthropic TypeScript SDK](https://github.com/anthropics/anthropic-sdk-typescript) (MIT), [Vite](https://vite.dev) and [vite-plugin-singlefile](https://github.com/richardtallent/vite-plugin-singlefile) (MIT), and the Fredoka, Atkinson Hyperlegible and JetBrains Mono fonts (SIL Open Font License) via [Fontsource](https://fontsource.org).
