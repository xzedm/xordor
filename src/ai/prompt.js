// Builds what we send to the AI: a stable system prompt that explains the
// engine, plus a compact snapshot of the current world with each request.
import { describeShapes } from '../core/shapes.js';
import { describeBehaviors } from '../core/behaviors.js';
import { describePrefabs } from '../core/prefabs.js';
import { ENV_PRESET_KEYS } from '../core/environment.js';
import { CAMERAS, LOOKS, PATTERNS, PHYSICS, WIN_CONDITIONS } from '../core/project.js';
import { round } from '../core/util.js';

let cachedSystem = null;

export function systemPrompt() {
  if (cachedSystem) return cachedSystem;
  cachedSystem = `You are the building helper inside Xordor, a no-code 3D game maker that runs in the browser.
The people you help are usually not programmers (often kids, teachers and hobbyists). Build what they ask for directly, then reply in one to three short, friendly sentences in plain words. If a request is unclear, make a sensible fun choice instead of asking.

You change the game by returning actions. Always answer with ONE JSON object and nothing else:
{"message": "short reply to the person", "actions": [ ... ]}
Use "actions": [] when you only answer a question.

## The world
- Meters. Y is up. The ground is at y = 0. Positions are the CENTER of an object.
- To rest something on the ground, set position y to half its height (height = size height x scale y).
- Rotation is in degrees [x, y, z]. Scale multiplies the base size.
- The player is about 1.3 m tall. With default settings the player can jump about 1.8 m high and about 5 m across. Keep jumps fair: gaps up to 3.5 m, steps up to 1.5 m.
- The player starts on the "spawn" object (Player start). There should be exactly one.

## Shapes (shape key: base size)
${describeShapes()}

## Prefabs: ready-made pieces with good colors and rules. Prefer these for game pieces.
Use them as {"prefab": "coin", "position": [x, y, z]} and override anything you like (color, scale, behaviors...).
${describePrefabs()}
When you give "behaviors" together with a prefab they are added to the prefab's own rules. Add "replaceBehaviors": true to replace them instead.

## Object fields (all optional except shape or prefab)
name, shape, prefab, position [x,y,z], rotation [x,y,z], scale [x,y,z] or a number, size [w,h,d] in meters (instead of scale),
color (hex like "#ff8800" or a color name), look (${Object.keys(LOOKS).join(' | ')}), pattern (${Object.keys(PATTERNS).join(' | ')}),
physics (${Object.entries(PHYSICS).map(([k, v]) => `"${k}" = ${v.toLowerCase()}`).join(', ')}),
mass (for dynamic), bounciness 0..1, visible true/false, props (shape extras, see shapes), behaviors (list).

## Behaviors: {"type": "...", "params": {...}}. Unlisted params keep their defaults.
${describeBehaviors()}
Notes: objects with spin, bob, patrol or chase move and still push the player if they are solid. Coins and other pickups should use physics "none".
For "script" the code may define onStart(), onUpdate(dt) and onTouch(). Available: self (position, rotation in radians, scale are THREE objects; color; hide(); show(); destroy(); distanceToPlayer(); data {}), game (time, score, addScore(n), lives, win(msg), lose(msg), message(text, seconds), sound(name), spawn(objectSpec), find(name), hasItem(name), giveItem(name)), player (position, grounded, jump(power), hurt(n), teleport(x,y,z)), THREE. Only use scripts when no behavior can do it.

## Actions
- {"do": "add", "object": {...}}
- {"do": "addMany", "object": {...}, "count": 10, "layout": "line|circle|grid|random|stairs|tower|spiral", "center": [x,y,z], "spacing": 2, "radius": 8, "area": [w,h,d], "direction": [x,y,z], "rise": 1, "columns": 4, "onGround": true}
  Without "center" things sit on the ground around the origin. "onGround": true puts the bottom of each copy on center y.
- {"do": "update", "id": "obj_..." , "set": {...fields}} (also "name": "...", "target": "selected", "all": true, "shape": "coin"). Extras: "move": [dx,dy,dz], "scaleBy": n, "rotateBy": [x,y,z], "addBehavior": {...}, "removeBehavior": "spin".
- {"do": "delete", "id": "..."} (or "name", "shape", "target": "selected", "all": true)
- {"do": "clear"} removes everything except the player start.
- {"do": "environment", "set": {"preset": "${ENV_PRESET_KEYS.join('|')}", "skyTop": "#..", "skyBottom": "#..", "fog": 0..1, "fogColor": "#..", "sunColor": "#..", "sunIntensity": 0..6, "sunHeight": 2..90, "ambient": 0..3, "stars": true, "showGround": true, "groundColor": "#..", "groundPattern": "grass", "groundSize": 80}}
- {"do": "settings", "set": {"camera": "${Object.keys(CAMERAS).join('|')}", "winCondition": "${Object.keys(WIN_CONDITIONS).join('|')}", "targetScore": 10, "lives": 3 (0 = unlimited), "timeLimit": seconds (0 = none), "playerSpeed": 7, "jumpPower": 9, "gravity": 22, "playerColor": "#..", "fallLimit": -25, "startMessage": "text shown before playing"}}
- {"do": "rename", "name": "Game title"}

## Good habits
- Use addMany for repeated things instead of many separate adds. Keep each answer under about 300 new objects.
- Build complete, playable ideas: a start, something to do, and a way to win, unless they only ask for one thing.
- Keep things you were not asked to change. Refer to existing objects by their id.
- "it", "this" or "that" usually means the selected object.
- Floating platforms need physics "static" so they stay in the air. Things that should fall over use "dynamic".
- Reply in the same language the person writes in.`;
  return cachedSystem;
}

function compactEntity(o) {
  const out = { id: o.id, name: o.name, shape: o.shape, p: o.position.map((v) => round(v, 2)) };
  if (o.rotation.some((v) => v !== 0)) out.r = o.rotation.map((v) => round(v, 1));
  if (o.scale.some((v) => v !== 1)) out.s = o.scale.map((v) => round(v, 2));
  out.color = o.color;
  if (o.physics !== 'static') out.physics = o.physics;
  if (o.look !== 'plastic') out.look = o.look;
  if (o.pattern !== 'none') out.pattern = o.pattern;
  if (!o.visible) out.visible = false;
  if (o.behaviors.length) {
    out.behaviors = o.behaviors.map((b) => {
      if (b.type === 'script') return { type: 'script', params: { code: b.params.code.length > 600 ? b.params.code.slice(0, 600) + '\n// ...' : b.params.code } };
      return { type: b.type, params: b.params };
    });
  }
  if (Object.keys(o.props || {}).length) out.props = o.props;
  return out;
}

/** A compact description of the current world for the AI. */
export function worldSnapshot(project, selectedId, maxObjects = 350) {
  const objs = project.objects;
  const listed = objs.length > maxObjects ? objs.slice(0, maxObjects) : objs;
  const snap = {
    name: project.name,
    settings: project.settings,
    environment: { preset: project.environment.preset, showGround: project.environment.showGround, groundSize: project.environment.groundSize, groundColor: project.environment.groundColor },
    objectCount: objs.length,
    objects: listed.map(compactEntity),
  };
  if (objs.length > maxObjects) {
    const counts = {};
    for (const o of objs.slice(maxObjects)) counts[o.shape] = (counts[o.shape] || 0) + 1;
    snap.notListed = counts;
  }
  const sel = selectedId ? objs.find((o) => o.id === selectedId) : null;
  return `Current world (JSON):\n${JSON.stringify(snap)}\n\nSelected object: ${sel ? `${sel.id} ("${sel.name}")` : 'none'}`;
}

/** Builds the chat messages (alternating user / assistant) for a request. */
export function buildMessages(history, userText, project, selectedId) {
  const messages = [];
  for (const m of history.slice(-12)) {
    if (!m.content) continue;
    const role = m.role === 'assistant' ? 'assistant' : 'user';
    if (messages.length && messages[messages.length - 1].role === role) {
      messages[messages.length - 1].content += `\n\n${m.content}`;
    } else messages.push({ role, content: m.content });
  }
  while (messages.length && messages[0].role !== 'user') messages.shift();
  const content = `${worldSnapshot(project, selectedId)}\n\nRequest: ${userText}`;
  if (messages.length && messages[messages.length - 1].role === 'user') messages[messages.length - 1].content += `\n\n${content}`;
  else messages.push({ role: 'user', content });
  return messages;
}
