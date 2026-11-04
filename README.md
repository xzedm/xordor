# Xordor

**Make 3D games in your web browser. No coding needed.**

Xordor is a game maker for everyone. You build worlds by clicking things together, give objects simple rules like "spin", "collect for points" or "chase the player", and press **Play**. An AI helper can build for you too. Type *"make a lava level with coins and a finish flag"* and it builds the level.

- Works in any modern browser (Chrome, Edge, Firefox, Safari), on computers, tablets and phones.
- Nothing to install. The whole app is a single file.
- An AI helper that works with **ChatGPT, Claude, Gemini, OpenRouter, DeepSeek, Mistral, Groq, Grok**, free local models (**Ollama**, **LM Studio**) or any OpenAI-compatible service. A free offline helper is built in.
- **Publish** your game as one HTML file that anyone can open to play.

---

## Getting started (no technical knowledge needed)

### 1. Open Xordor

Pick whichever is easiest:

- **Use the website.** When this repository has GitHub Pages switched on (see [Putting Xordor online](#putting-xordor-online)), open `https://<your-github-name>.github.io/xordor/`.
- **Use the file.** Download `index.html` from the latest build (in this repository's **Actions** tab, open the newest run and download **xordor-app**), then double-click it. It opens in your browser and works offline.

### 2. Pick a starting game

The first time you open Xordor you can choose a starter: *Empty world*, *Coin hunt*, *Sky parkour*, *Enemy arena*, *Physics playground* or *Maze escape*. Every one of them can be changed completely.

### 3. Build

| To do this | Do this |
|---|---|
| Add something | Click it in the **Add** panel on the left. It appears in the middle of your view. |
| Select something | Click it in the 3D view (or in the **Objects** list). |
| Move, turn or resize | Drag the colored arrows. Switch tools with **Move**, **Turn** and **Size** at the bottom, or press **W**, **E** or **R**. |
| Change color, material or pattern | Use the **Properties** panel on the right. |
| Make it do something | In **Properties**, under **Behaviors**, pick one. For example *Spin*, *Move back and forth*, *Collect for points*, *Hurts the player*, *Bounce pad* or *Finish line*. |
| Change the sky, ground and rules | Click empty space so nothing is selected. The **Properties** panel then shows the world settings and the game rules. |
| Look around | Drag with the mouse. Right-drag to slide. Scroll to zoom. Double-click to zoom to the selected thing. |
| Undo a mistake | **Ctrl+Z** (or the arrow button in the top bar). **Ctrl+Shift+Z** redoes. |

### 4. Play

Press the green **Play** button.

- **W A S D** or the arrow keys move, **Space** jumps, hold **Shift** to run.
- Drag the mouse to look around. **Q** and **E** turn the camera.
- **Esc** takes you back to the editor.
- On phones and tablets, use the on-screen stick and the **Jump** button.

### 5. Save and share

- Your game **saves automatically** in your browser.
- **Save** downloads a `.xordor.json` file. Keep it as a backup, or send it to a friend so they can open it with **Open**.
- **Publish** downloads a playable `.html` file. Anyone can double-click it to play, with no install and no internet. You can also upload it to a free host such as GitHub Pages, Netlify Drop or itch.io (choose "HTML game").

---

## The AI helper

The AI helper sits on the right side of the screen. Type what you want in plain words:

- *"Build an obstacle course with moving platforms and a finish flag"*
- *"Add 10 coins in a circle around the house"*
- *"Make it night and add some lights"*
- *"Make enemies slower"*
- *"Make this bounce"* (select an object first to change just that object)

Every change the AI makes can be undone with one click (**Undo this** under its reply).

### Choosing an AI

Click the gear button at the top of the AI panel. Then pick a service:

| Service | What you need | Where to get it |
|---|---|---|
| **Built-in helper** | Nothing. Free and works offline. It understands simple requests only. | Already included |
| **ChatGPT (OpenAI)** | An OpenAI API key | [platform.openai.com/api-keys](https://platform.openai.com/api-keys) |
| **Claude (Anthropic)** | An Anthropic API key | [platform.claude.com/settings/keys](https://platform.claude.com/settings/keys) |
| **Gemini (Google)** | A Google AI Studio key | [aistudio.google.com/apikey](https://aistudio.google.com/apikey) |
| **OpenRouter** | One key for hundreds of models | [openrouter.ai/keys](https://openrouter.ai/keys) |
| **DeepSeek, Mistral, Groq, Grok** | That company's API key | Each company's developer site |
| **Ollama** (free, runs on your computer) | Install [Ollama](https://ollama.com/download), download a model (for example `ollama pull llama3.2`), then start it so web pages may connect: `OLLAMA_ORIGINS=* ollama serve` | [ollama.com](https://ollama.com) |
| **LM Studio** (free, runs on your computer) | Start LM Studio's local server and turn on CORS | [lmstudio.ai](https://lmstudio.ai) |
| **Other** | Any service with an OpenAI-compatible `/chat/completions` address | Your provider |

Paste the key and press **Save**. Press **Load models** to see which models your account can use.

**Things to know**

- An API key is different from a ChatGPT Plus or Claude Pro subscription. API use is billed per request by the company you pick. A typical request costs a fraction of a cent to a few cents.
- Your key is stored **only in this browser** and sent **only to the service you picked**. Published games never contain your keys.
- Each request sends a short description of your world (object names, shapes, positions and rules) so the AI knows what is there.
- Some services don't allow requests made directly from a web page. If one won't connect, try OpenRouter, which offers many of the same models.

---

## What you can build with

**Game pieces:** player start, coins, stars, gems, extra lives, keys and locked doors, finish flags, checkpoints, enemies, spikes, lava, bounce pads, speed pads, portals, moving platforms, spinning bars, falling platforms, ball droppers.

**Building:** platforms, walls, pillars, stairs, ramps, houses, crates, barrels, bouncy balls, signs, water.

**Nature:** trees, pine trees, rocks, bushes, clouds. **Lights:** glowing lamps. **Basic shapes:** cube, ball, cylinder, cone, pyramid, capsule, ring.

**Behaviors:** spin, float up and down, move back and forth, chase the player, collect for points, hurts the player, finish line, checkpoint, bounce pad, speed boost, teleporter, vanish when touched, show a message, locked door, drop or throw balls, and custom code (the AI can write this for you).

**Worlds:** sunny day, sunset, night, outer space, foggy, desert, snowy, candy land and volcano, or your own sky, sun and fog colors.

**Rules:** camera behind the player, first person or from above; win by reaching the finish, collecting everything, reaching a score, or surviving a timer; lives, time limit, running speed, jump power and gravity.

---

## Putting Xordor online

This repository has a ready-made GitHub Actions workflow that builds Xordor and publishes it with GitHub Pages:

1. On GitHub, open the repository's **Settings**, then **Pages**.
2. Under **Build and deployment**, set **Source** to **GitHub Actions**.
3. Push to the `main` branch (or run the **Build and deploy** workflow from the **Actions** tab).

Your copy of Xordor is then at `https://<your-github-name>.github.io/<repository-name>/`.

---

## For developers

Xordor is plain JavaScript (no framework), built with [Vite](https://vite.dev). It needs Node.js 20.19 or newer (22 recommended).

```bash
npm install        # install dependencies
npm run dev        # start a local dev server with live reload
npm test           # run the unit tests (Vitest)
npm run build      # build dist/index.html, one self-contained file
```

Publishing games works in the built file (`dist/index.html`). It copies the app's own bundled code into the exported game, so it is not available under `npm run dev`.

### How it fits together

```
src/
  core/            Plain data, no 3D needed (unit tested in Node)
    project.js       The project format, defaults and validation
    shapes.js        Every shape: friendly info plus how to build it
    prefabs.js       The Add palette (shapes with ready-made settings and rules)
    behaviors.js     Behavior names, settings and defaults
    environment.js   Sky and mood presets
  engine/          Runs the 3D world
    world.js         Turns project data into a Three.js scene (shared by editor and game)
    materials.js     Materials and procedurally drawn textures (no image files)
    physics.js       cannon-es bodies built from each shape's parts
    Game.js          Play mode: player, camera, rules, HUD, win and lose
    behaviorRuntime.js  What each behavior does while playing
    input.js, audio.js  Keyboard, mouse and touch, plus synthesized sound effects
  editor/          The studio interface
    Editor.js        Project state, undo and redo, selection, gizmos
    panels.js        Add, Objects and Properties panels
    aiPanel.js       AI chat and AI settings
    storage.js       Autosave, file open and save, publishing
  ai/              The AI layer
    providers.js     OpenAI-compatible, Anthropic (official SDK) and Gemini clients
    prompt.js        System prompt (generated from the catalogs) and world snapshot
    parse.js         Forgiving JSON reader for model replies
    actions.js       Applies AI actions to a project (pure, one undo step)
    offline.js       The rule-based built-in helper
  templates.js     Starter games, built with the same actions the AI uses
```

**The AI never touches the 3D scene directly.** It returns JSON actions (`add`, `addMany`, `update`, `delete`, `environment`, `settings`...). `applyActions()` checks and cleans them into a new project, which the editor commits as a single undo step. That keeps AI changes safe, reviewable and undoable. The system prompt is generated from the shape, prefab and behavior catalogs, so adding a new shape or behavior automatically teaches the AI about it.

**Adding a shape:** add an entry to `SHAPES` in `src/core/shapes.js` (a label, a base size and a `build` function that returns mesh parts; mark solid parts with `collider`). Add it to the palette in `src/core/prefabs.js` if you want a button for it.

**Adding a behavior:** describe it in `src/core/behaviors.js` (label, settings and defaults) and implement it in `src/engine/behaviorRuntime.js`. The Properties panel builds its form from the description automatically.

**Claude settings:** the Anthropic client uses the official `@anthropic-ai/sdk` in browser mode, streams replies, sets `output_config.effort` (Fast, Balanced or Thorough in AI settings), caches the system prompt, and opts into server-side refusal fallbacks (`fallbacks: "default"`) on models that support them.

### Security notes

- API keys stay in the browser's local storage and are only sent to the provider the user picked.
- Custom code behaviors run JavaScript while playing. When someone opens a project file that contains custom code, Xordor asks whether to keep or remove it.

---

## Credits

Xordor is built on these open-source projects:

- [three.js](https://threejs.org): 3D rendering (MIT license)
- [cannon-es](https://github.com/pmndrs/cannon-es): physics (MIT license)
- [Anthropic TypeScript SDK](https://github.com/anthropics/anthropic-sdk-typescript): Claude API client (MIT license)
- [Vite](https://vite.dev) and [vite-plugin-singlefile](https://github.com/richardtallent/vite-plugin-singlefile): building (MIT license)
- Fonts: Fredoka, Atkinson Hyperlegible and JetBrains Mono (SIL Open Font License), loaded from Google Fonts when online
