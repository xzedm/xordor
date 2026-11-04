// The built-in helper: a small, rule-based assistant that works without any
// AI service. It understands simple requests and turns them into the same
// actions the online AI uses. Connect ChatGPT, Claude or Gemini for more.
import { Color } from 'three';
import { seededRandom } from '../core/util.js';

const NUMBER_WORDS = {
  a: 1, an: 1, one: 1, two: 2, three: 3, four: 4, five: 5, six: 6, seven: 7, eight: 8, nine: 9, ten: 10,
  eleven: 11, twelve: 12, fifteen: 15, twenty: 20, thirty: 30, fifty: 50, few: 3, couple: 2, some: 5, several: 5, lots: 15, many: 12, dozen: 12,
};

// Phrase -> prefab key. Longer phrases are checked first.
const THINGS = [
  ['race car', 'car'], ['car', 'car'], ['vehicle', 'car'], ['turret', 'turret'], ['cannon tower', 'turret'], ['target', 'target'],
  ['moving platform', 'mover'], ['falling platform', 'crumble'], ['spinning bar', 'spinner'], ['spinner', 'spinner'],
  ['ball dropper', 'dropper'], ['bouncy ball', 'bouncyball'], ['bounce pad', 'bouncepad'], ['trampoline', 'bouncepad'],
  ['jump pad', 'bouncepad'], ['speed pad', 'speedpad'], ['speed boost', 'speedpad'], ['player start', 'start'],
  ['spawn', 'start'], ['pine tree', 'pine'], ['pine', 'pine'], ['extra life', 'heart'], ['finish line', 'goal'],
  ['finish', 'goal'], ['goal', 'goal'], ['flag', 'goal'], ['checkpoint', 'checkpoint'], ['teleporter', 'portal'], ['portal', 'portal'],
  ['platform', 'platform'], ['wall', 'wall'], ['pillar', 'pillar'], ['column', 'pillar'], ['stairs', 'stairs'], ['staircase', 'stairs'],
  ['ramp', 'ramp'], ['house', 'house'], ['crate', 'crate'], ['barrel', 'barrel'], ['sign', 'sign'], ['water', 'water'], ['pool', 'water'],
  ['tree', 'tree'], ['rock', 'rock'], ['stone', 'rock'], ['boulder', 'rock'], ['bush', 'bush'], ['cloud', 'cloud'],
  ['coin', 'coin'], ['star', 'star'], ['gem', 'gem'], ['diamond', 'gem'], ['crystal', 'gem'], ['heart', 'heart'], ['key', 'key'],
  ['door', 'door'], ['enemy', 'enemy'], ['enemies', 'enemy'], ['monster', 'enemy'], ['bad guy', 'enemy'], ['spike', 'spikes'], ['lava', 'lava'],
  ['light', 'lamp'], ['lamp', 'lamp'], ['cube', 'cube'], ['box', 'cube'], ['block', 'cube'], ['ball', 'ball'], ['sphere', 'ball'],
  ['cylinder', 'cylinder'], ['cone', 'cone'], ['pyramid', 'pyramid'], ['capsule', 'capsule'], ['ring', 'ring'], ['donut', 'ring'],
];

const PREFAB_SHAPE = {
  mover: 'box', crumble: 'box', spinner: 'box', dropper: 'cloud', bouncyball: 'sphere', bouncepad: 'bouncepad', speedpad: 'speedpad',
  car: 'car', turret: 'turret', target: 'target', start: 'spawn', pine: 'pine', heart: 'heart', goal: 'flag', checkpoint: 'checkpoint', portal: 'portal', platform: 'box', wall: 'box',
  pillar: 'cylinder', stairs: 'stairs', ramp: 'ramp', house: 'house', crate: 'box', barrel: 'cylinder', sign: 'sign', water: 'box',
  tree: 'tree', rock: 'rock', bush: 'bush', cloud: 'cloud', coin: 'coin', star: 'star', gem: 'gem', key: 'key', door: 'box', enemy: 'enemy',
  spikes: 'spikes', lava: 'box', lamp: 'light', cube: 'box', ball: 'sphere', cylinder: 'cylinder', cone: 'cone', pyramid: 'pyramid',
  capsule: 'capsule', ring: 'torus',
};

const MOODS = [
  [/\b(night|dark|midnight|nighttime)\b/, 'night'], [/\b(sunset|dusk|evening)\b/, 'sunset'], [/\b(space|stars|galaxy|moon)\b/, 'space'],
  [/\b(fog|foggy|mist|misty)\b/, 'foggy'], [/\b(desert|sand|sandy)\b/, 'desert'], [/\b(snow|snowy|winter|ice|icy)\b/, 'snow'],
  [/\b(candy|sweet|pink world)\b/, 'candy'], [/\b(volcano|hell|lava world)\b/, 'lava'], [/\b(day|daytime|sunny|morning)\b/, 'day'],
];

function findColor(text) {
  const words = text.match(/[a-z]+/g) || [];
  for (let i = 0; i < words.length; i++) {
    const two = words[i] + (words[i + 1] || '');
    if (Color.NAMES[two] !== undefined && words[i + 1]) return '#' + new Color(Color.NAMES[two]).getHexString();
    if (['light', 'dark'].includes(words[i])) continue;
    if (Color.NAMES[words[i]] !== undefined && !['tan', 'linen', 'snow'].includes(words[i])) return '#' + new Color(Color.NAMES[words[i]]).getHexString();
  }
  const hex = text.match(/#[0-9a-f]{6}\b/);
  return hex ? hex[0] : null;
}

function findCount(text) {
  const digits = text.match(/\b(\d{1,3})\b/);
  if (digits) return Math.min(300, parseInt(digits[1], 10));
  for (const w of text.match(/[a-z]+/g) || []) if (NUMBER_WORDS[w] !== undefined) return NUMBER_WORDS[w];
  return 1;
}

function findThing(text) {
  for (const [phrase, key] of THINGS) {
    const re = new RegExp(`\\b${phrase}(e?s)?\\b`);
    if (re.test(text)) return key;
  }
  return null;
}

function findLayout(text) {
  if (/\b(circle|ring|around me|round)\b/.test(text)) return 'circle';
  if (/\b(grid|rows and columns)\b/.test(text)) return 'grid';
  if (/\b(tower|stack|on top of each other|pile)\b/.test(text)) return 'tower';
  if (/\b(line|row)\b/.test(text)) return 'line';
  if (/\b(random|everywhere|scatter|scattered|all over|around)\b/.test(text)) return 'random';
  return null;
}

const HELP = 'I can help with simple requests, for example: "add 10 coins in a circle", "make it night", "add an enemy", "make it bigger", "make it spin", "build an obstacle course", "build a maze" or "delete all trees". For anything more creative, connect ChatGPT, Claude or Gemini in AI settings.';

/**
 * Turns a request into { message, actions }.
 * ctx: { selectedId, selectedName, focus: [x, y, z] }
 */
export function offlineAssistant(input, ctx = {}) {
  const text = ` ${String(input || '').toLowerCase().replace(/[^\w#\s'-]/g, ' ')} `;
  const actions = [];
  const notes = [];
  const focus = ctx.focus || [0, 0, 0];
  const sel = ctx.selectedId;
  const color = findColor(text);

  if (/\b(help|what can you do|how do i|how does)\b/.test(text) || /^\s*\?+\s*$/.test(input || '')) {
    return { message: HELP, actions: [] };
  }

  // Start over.
  if (/\b(clear|delete|remove|erase) (everything|all|it all|the whole)\b|\bstart over\b|\bempty (the )?world\b/.test(text) && !findThing(text)) {
    return { message: 'Cleared the world. The player start is still there.', actions: [{ do: 'clear' }] };
  }

  // Big builds.
  if (/\b(obstacle course|parkour|platformer|jump(ing)? course)\b/.test(text)) {
    return { message: 'Here is an obstacle course: jump across the platforms, grab the coins and reach the flag!', actions: obstacleCourse(focus, color) };
  }
  if (/\b(race track|racetrack|racing|race game|rally)\b/.test(text)) {
    return { message: 'Built a race: drive through the checkpoints to the finish before time runs out!', actions: raceTrack(focus) };
  }
  if (/\b(shooting range|shooting gallery|shooter game|robot attack)\b/.test(text)) {
    return { message: 'Built a shooting range: hit every target!', actions: shootingRange(focus) };
  }
  if (/\bmaze\b/.test(text)) {
    return { message: 'Built a maze! Find your way to the flag in the far corner.', actions: maze(focus, color) };
  }
  if (/\b(castle|fort|fortress)\b/.test(text)) {
    return { message: 'Built a little castle with towers.', actions: castle(focus, color) };
  }
  if (/\bforest\b|\bwoods\b/.test(text)) {
    const n = Math.max(8, Math.min(80, findCount(text) > 1 ? findCount(text) : 25));
    return { message: `Planted a forest of ${n} trees.`, actions: [{ do: 'addMany', object: { prefab: /pine/.test(text) ? 'pine' : 'tree' }, count: n, layout: 'random', area: [40, 0, 40], center: [focus[0], 0, focus[2]], onGround: true, seed: Math.floor(Math.random() * 1e6) }] };
  }

  // Sky and mood.
  for (const [re, preset] of MOODS) {
    if (re.test(text) && /\b(make|set|change|turn|switch|it|world|sky|time)\b/.test(text) && !findThing(text.replace(re, ''))) {
      actions.push({ do: 'environment', set: { preset } });
      notes.push(`Switched the world to ${preset === 'lava' ? 'a volcano' : preset}.`);
      return { message: notes.join(' '), actions };
    }
  }

  // Camera and rules.
  if (/\b(rapid fire|machine gun|minigun)\b/.test(text)) { actions.push({ do: 'settings', set: { weapon: 'rapid' } }); notes.push('The player has a rapid-fire weapon now. Shoot with the mouse or K.'); }
  else if (/\b(cannon|bazooka|rocket)\b/.test(text) && /\b(give|weapon|shoot|player|gun)\b/.test(text)) { actions.push({ do: 'settings', set: { weapon: 'cannon' } }); notes.push('The player has a cannon now. Shoot with the mouse or K.'); }
  else if (/\b(gun|blaster|weapon|shooting|shoot)\b/.test(text) && !/\b(no|remove|without|stop|turn off)\b/.test(text) && !findThing(text)) { actions.push({ do: 'settings', set: { weapon: 'blaster' } }); notes.push('The player can shoot now: left mouse button or K. Things need "Can be destroyed" to be shot.'); }
  if (/\b(no|remove|without|stop|turn off) (the )?(gun|weapon|shooting)\b/.test(text)) { actions.push({ do: 'settings', set: { weapon: 'none' } }); notes.push('Shooting is off.'); }
  if (/\bfirst[- ]person\b/.test(text)) { actions.push({ do: 'settings', set: { camera: 'first-person' } }); notes.push('The game now uses a first-person camera.'); }
  if (/\bthird[- ]person\b/.test(text)) { actions.push({ do: 'settings', set: { camera: 'third-person' } }); notes.push('The camera now follows behind the player.'); }
  if (/\btop[- ]down\b|\bfrom above\b/.test(text)) { actions.push({ do: 'settings', set: { camera: 'top-down' } }); notes.push('The camera now looks down from above.'); }
  const timer = text.match(/\b(timer|time limit|seconds?)\D{0,10}(\d+)|(\d+)\s*(seconds?|secs?)\b/);
  if (timer && /\b(timer|time|limit|seconds?)\b/.test(text) && !findThing(text)) {
    const secs = parseInt(timer[2] || timer[3], 10);
    actions.push({ do: 'settings', set: { timeLimit: secs } });
    notes.push(`Added a ${secs} second timer.`);
  }
  const lives = text.match(/\b(\d+)\s+lives\b/);
  if (lives) { actions.push({ do: 'settings', set: { lives: parseInt(lives[1], 10) } }); notes.push(`The player now has ${lives[1]} lives.`); }
  if (/\b(infinite|unlimited|endless) lives\b/.test(text)) { actions.push({ do: 'settings', set: { lives: 0 } }); notes.push('Lives are now unlimited.'); }
  if (/\blow gravity\b|\bmoon gravity\b|\bfloaty\b/.test(text)) { actions.push({ do: 'settings', set: { gravity: 8, jumpPower: 8 } }); notes.push('Gravity is now low and floaty.'); }
  if (/\bjump higher\b|\bhigher jump/.test(text)) { actions.push({ do: 'settings', set: { jumpPower: 13 } }); notes.push('The player jumps higher now.'); }
  if (/\b(faster|run faster|speed up) (player|me)\b|\bplayer faster\b/.test(text)) { actions.push({ do: 'settings', set: { playerSpeed: 11 } }); notes.push('The player runs faster now.'); }
  if (/\bcollect (all|everything)\b/.test(text) && /\bwin\b/.test(text)) { actions.push({ do: 'settings', set: { winCondition: 'collect-all' } }); notes.push('Collect everything to win.'); }
  if (actions.length) return { message: notes.join(' '), actions };

  const thing = findThing(text);
  const wantsRemove = /\b(delete|remove|get rid of|erase|destroy)\b/.test(text);

  // Changes to the selected object.
  const refersToSelection = /\b(it|this|that|selected|the object)\b/.test(text) || !thing;
  if (sel && refersToSelection && !wantsRemove && !/\b(add|create|make (a|an|some|\d+)|place|put|spawn|build)\b/.test(text)) {
    const set = {};
    const change = { do: 'update', id: sel };
    const said = [];
    if (/\b(bigger|larger|grow|huge|giant)\b/.test(text)) { change.scaleBy = /\b(huge|giant)\b/.test(text) ? 3 : 1.5; said.push('made it bigger'); }
    if (/\b(smaller|shrink|tiny|little)\b/.test(text)) { change.scaleBy = /\btiny\b/.test(text) ? 0.3 : 0.67; said.push('made it smaller'); }
    if (/\btaller\b/.test(text)) { change.scaleBy = [1, 1.5, 1]; said.push('made it taller'); }
    if (/\bwider\b/.test(text)) { change.scaleBy = [1.5, 1, 1.5]; said.push('made it wider'); }
    if (color) { set.color = color; said.push('changed the color'); }
    const adds = [];
    if (/\b(spin|rotate|turn around|twirl)\b/.test(text)) { adds.push({ type: 'spin' }); said.push('it spins now'); }
    if (/\b(float|bob|hover)\b/.test(text)) { adds.push({ type: 'bob' }); said.push('it floats now'); }
    if (/\b(back and forth|patrol|slide|move around)\b/.test(text)) { adds.push({ type: 'patrol' }); said.push('it moves back and forth'); }
    if (/\b(chase|follow)\b/.test(text)) { adds.push({ type: 'chase' }); said.push('it chases the player'); }
    if (/\b(collectible|collect|pick up|pickup)\b/.test(text)) { adds.push({ type: 'collectible' }); set.physics = 'none'; said.push('it can be collected'); }
    if (/\b(deadly|hurt|hurts|dangerous|kill|kills)\b/.test(text)) { adds.push({ type: 'hazard' }); said.push('it hurts the player'); }
    if (/\b(bouncy|bounce)\b/.test(text)) { set.physics = 'dynamic'; set.bounciness = 0.95; said.push('it is bouncy'); }
    if (/\b(fall|falls|physics|pushable|push|knock)\b/.test(text)) { set.physics = 'dynamic'; said.push('it can fall and be pushed'); }
    if (/\b(ghost|walk through|pass through)\b/.test(text)) { set.physics = 'none'; said.push('you can walk through it'); }
    if (/\b(solid)\b/.test(text)) { set.physics = 'static'; said.push('it is solid'); }
    if (/\b(glow|glowing|shine|neon)\b/.test(text)) { set.look = 'glow'; said.push('it glows'); }
    if (/\b(metal|metallic|shiny)\b/.test(text)) { set.look = 'metal'; said.push('it looks like metal'); }
    if (/\b(glass|transparent|see through)\b/.test(text)) { set.look = 'glass'; said.push('it looks like glass'); }
    if (/\b(invisible|hide|hidden)\b/.test(text)) { set.visible = false; said.push('it is invisible while playing'); }
    if (/\b(visible|show)\b/.test(text) && !set.visible) { set.visible = true; said.push('it is visible'); }
    if (/\b(stop|no more) (spinning|moving|floating)\b/.test(text)) { change.removeBehavior = ['spin', 'patrol', 'bob', 'chase']; said.push('it stays still now'); }
    if (/\bup\b/.test(text) && /\bmove\b/.test(text)) { change.move = [0, 1, 0]; said.push('moved it up'); }
    if (/\bdown\b/.test(text) && /\bmove\b/.test(text)) { change.move = [0, -1, 0]; said.push('moved it down'); }
    if (adds.length) change.addBehavior = adds;
    if (Object.keys(set).length) change.set = set;
    if (said.length) {
      const msg = said.join(', ');
      return { message: `Done: ${msg}.`.replace('Done: it', 'Done, it'), actions: [change] };
    }
  }

  // Remove things.
  if (wantsRemove) {
    if (thing && /\b(all|every|the)\b/.test(text) && !/\b(this|that|it|selected)\b/.test(text)) {
      return { message: `Removed the ${thing === 'cube' ? 'cubes' : thing + 's'}.`, actions: [{ do: 'delete', shape: PREFAB_SHAPE[thing] || thing, all: true }] };
    }
    if (sel) return { message: `Removed ${ctx.selectedName || 'the selected object'}.`, actions: [{ do: 'delete', id: sel }] };
    return { message: 'Select something first, or say what to remove, like "delete all coins".', actions: [] };
  }

  // Add things.
  if (thing) {
    let count = findCount(text);
    const naturallyPlural = thing === 'spikes' || thing === 'stairs';
    const said = THINGS.map(([phrase, key]) => (key === thing ? text.match(new RegExp(`\\b${phrase}(e?s)?\\b`))?.[0] : null)).find(Boolean) || '';
    if (count === 1 && !naturallyPlural && /s$/.test(said) && !/\b(a|an|one)\b/.test(text)) count = 5;
    const object = { prefab: thing };
    if (color) object.color = color;
    if (/\b(giant|huge|big)\b/.test(text)) object.scaleMultiplier = 2.5;
    if (/\b(tiny|small|little)\b/.test(text)) object.scaleMultiplier = 0.5;
    if (/\b(spinning|spin)\b/.test(text)) object.behaviors = ['spin'];
    if (/\b(floating|float)\b/.test(text)) object.behaviors = [...(object.behaviors || []), 'bob'];
    if (/\b(bouncy)\b/.test(text)) { object.physics = 'dynamic'; object.bounciness = 0.95; }
    const scaled = applyScaleMultiplier(object);
    const layout = findLayout(text) || (count > 1 ? 'random' : null);
    const center = [focus[0], 0, focus[2]];
    if (count === 1) {
      return { message: `Added a ${thing === 'start' ? 'player start' : labelFor(thing)}.`, actions: [{ do: 'add', object: { ...scaled, near: center } }] };
    }
    const area = Math.max(10, Math.sqrt(count) * 4);
    return {
      message: `Added ${count} ${labelFor(thing)}${count > 1 ? 's' : ''}${layout === 'circle' ? ' in a circle' : layout === 'tower' ? ' stacked up' : ''}.`,
      actions: [{ do: 'addMany', object: scaled, count, layout, center, area: [area, 0, area], spacing: layout === 'tower' ? 1.02 : 2.5, radius: Math.max(4, count * 0.6), onGround: true, seed: Math.floor(Math.random() * 1e6) }],
    };
  }

  if (/\b(hi|hello|hey|yo)\b/.test(text)) return { message: `Hi! ${HELP}`, actions: [] };
  return { message: `Sorry, I didn't understand that. ${HELP}`, actions: [] };
}

function labelFor(key) {
  const names = { cube: 'cube', ball: 'ball', mover: 'moving platform', crumble: 'falling platform', spinner: 'spinning bar', dropper: 'ball dropper', bouncyball: 'bouncy ball', bouncepad: 'bounce pad', speedpad: 'speed pad', start: 'player start', heart: 'extra life', goal: 'finish flag', lamp: 'light', pine: 'pine tree', spikes: 'spike trap', lava: 'lava pool' };
  return names[key] || key;
}

function applyScaleMultiplier(object) {
  const out = { ...object };
  if (out.scaleMultiplier) {
    out.scaleBy = out.scaleMultiplier;
    delete out.scaleMultiplier;
  }
  return out;
}

function obstacleCourse(focus, color) {
  const rand = seededRandom(Math.floor(Math.random() * 1e6));
  const actions = [{ do: 'environment', set: { preset: 'sunset' } }, { do: 'settings', set: { winCondition: 'goal', lives: 3 } }];
  let x = focus[0];
  let y = 0.6;
  let z = focus[2] - 6;
  const platformColor = color || '#7fb3d5';
  for (let i = 0; i < 10; i++) {
    const moving = i === 4 || i === 7;
    actions.push({ do: 'add', object: { prefab: moving ? 'mover' : 'platform', position: [x, y, z], scale: [3, 0.5, 3], color: moving ? undefined : platformColor, behaviors: moving ? [{ type: 'patrol', params: { axis: 'x', distance: 4, speed: 1.6 } }] : undefined } });
    actions.push({ do: 'add', object: { prefab: 'coin', position: [x + (moving ? 2 : 0), y + 1.2, z] } });
    x += (rand() - 0.5) * 4;
    y += 0.4 + rand() * 0.6;
    z -= 4.2 + rand() * 0.8;
  }
  actions.push({ do: 'add', object: { prefab: 'platform', position: [x, y, z], scale: [5, 0.5, 5], color: '#ffd166' } });
  actions.push({ do: 'add', object: { prefab: 'goal', position: [x, y + 1.5, z] } });
  actions.push({ do: 'add', object: { prefab: 'checkpoint', position: [focus[0], 0.85, focus[2] - 3] } });
  actions.push({ do: 'add', object: { prefab: 'lava', position: [focus[0], 0.15, (focus[2] - 6 + z) / 2], scale: [30, 0.3, Math.abs(z - focus[2]) + 10] } });
  return actions.map((a) => {
    if (a.object) for (const k of Object.keys(a.object)) if (a.object[k] === undefined) delete a.object[k];
    return a;
  });
}

function maze(focus, color) {
  const n = 7;
  const cell = 4;
  const rand = seededRandom(Math.floor(Math.random() * 1e6));
  const walls = new Set();
  for (let r = 0; r < n; r++) for (let c = 0; c < n; c++) { walls.add(`h${r},${c}`); walls.add(`v${r},${c}`); }
  const seen = new Set(['0,0']);
  const stack = [[0, 0]];
  while (stack.length) {
    const [r, c] = stack[stack.length - 1];
    const opts = [[r - 1, c, `h${r},${c}`], [r + 1, c, `h${r + 1},${c}`], [r, c - 1, `v${r},${c}`], [r, c + 1, `v${r},${c + 1}`]]
      .filter(([rr, cc]) => rr >= 0 && cc >= 0 && rr < n && cc < n && !seen.has(`${rr},${cc}`));
    if (!opts.length) { stack.pop(); continue; }
    const [rr, cc, wall] = opts[Math.floor(rand() * opts.length)];
    walls.delete(wall);
    seen.add(`${rr},${cc}`);
    stack.push([rr, cc]);
  }
  const ox = focus[0] - (n * cell) / 2;
  const oz = focus[2] - (n * cell) / 2 - 4;
  const wallColor = color || '#8fa3b8';
  const actions = [];
  for (const w of walls) {
    const [r, c] = w.slice(1).split(',').map(Number);
    if (w[0] === 'h' && r === 0 && c === 0) continue; // entrance
    if (w[0] === 'h') actions.push({ do: 'add', object: { prefab: 'wall', position: [ox + c * cell + cell / 2, 1.5, oz + r * cell], scale: [cell + 0.5, 3, 0.5], color: wallColor, pattern: 'bricks' } });
    else actions.push({ do: 'add', object: { prefab: 'wall', position: [ox + c * cell, 1.5, oz + r * cell + cell / 2], scale: [0.5, 3, cell + 0.5], color: wallColor, pattern: 'bricks' } });
  }
  for (let r = 0; r < n; r++) actions.push({ do: 'add', object: { prefab: 'wall', position: [ox + n * cell, 1.5, oz + r * cell + cell / 2], scale: [0.5, 3, cell + 0.5], color: wallColor, pattern: 'bricks' } });
  for (let c = 0; c < n; c++) actions.push({ do: 'add', object: { prefab: 'wall', position: [ox + c * cell + cell / 2, 1.5, oz + n * cell], scale: [cell + 0.5, 3, 0.5], color: wallColor, pattern: 'bricks' } });
  actions.push({ do: 'add', object: { prefab: 'goal', position: [ox + (n - 0.5) * cell, 1.25, oz + (n - 0.5) * cell] } });
  actions.push({ do: 'addMany', object: { prefab: 'coin' }, count: 8, layout: 'random', center: [focus[0], 0, oz + (n * cell) / 2], area: [n * cell - 4, 0, n * cell - 4], onGround: true, seed: 3 });
  actions.push({ do: 'add', object: { prefab: 'start', position: [ox + cell / 2, 0.06, oz - 2] } });
  actions.push({ do: 'settings', set: { winCondition: 'goal' } });
  return actions;
}

export function raceTrack(focus = [0, 0, 0]) {
  const [x, , z] = focus;
  const actions = [
    { do: 'environment', set: { preset: 'desert', groundSize: 640 } },
    { do: 'settings', set: { winCondition: 'goal', timeLimit: 75, lives: 0, camera: 'third-person' } },
    { do: 'add', object: { prefab: 'start', position: [x, 0.06, z] } },
    { do: 'add', object: { prefab: 'car', name: 'Race car', position: [x, 0.8, z - 4], rotation: [0, 180, 0], behaviors: [{ type: 'vehicle', params: { startInside: true, speed: 34 } }] } },
  ];
  // A winding route of checkpoints heading away from the start (-z).
  const route = [[0, -40], [25, -80], [10, -130], [-25, -170], [-10, -220], [20, -260]];
  route.forEach(([dx, dz], i) => {
    const px = x + dx;
    const pz = z + dz;
    if (i < route.length - 1) actions.push({ do: 'add', object: { prefab: 'checkpoint', name: `Checkpoint ${i + 1}`, position: [px, 2.55, pz], scale: [3, 3, 3] } });
    // Barrels mark both sides of each gate.
    actions.push({ do: 'add', object: { prefab: 'barrel', position: [px - 6, 0.55, pz], physics: 'dynamic' } });
    actions.push({ do: 'add', object: { prefab: 'barrel', position: [px + 6, 0.55, pz], physics: 'dynamic' } });
  });
  const [fx, fz] = route[route.length - 1];
  actions.push({ do: 'add', object: { prefab: 'goal', name: 'Finish', position: [x + fx, 3.75, z + fz - 4], scale: [3, 3, 3] } });
  actions.push({ do: 'add', object: { prefab: 'ramp', position: [x + 12, 1.2, z - 60], scale: [6, 2.4, 10], rotation: [0, 160, 0] } });
  actions.push({ do: 'addMany', object: { prefab: 'crate' }, count: 6, layout: 'tower', center: [x + 2, 0.5, z - 105], spacing: 1.01 });
  actions.push({ do: 'add', object: { prefab: 'speedpad', position: [x + 22, 0.04, z - 150], scale: [4, 1, 4] } });
  actions.push({ do: 'addMany', object: { prefab: 'rock' }, count: 18, layout: 'random', center: [x, 0, z - 140], area: [160, 0, 300], onGround: true, seed: 11 });
  return actions;
}

export function shootingRange(focus = [0, 0, 0]) {
  const [x, , z] = focus;
  return [
    { do: 'settings', set: { weapon: 'blaster', winCondition: 'defeat-all', lives: 3, timeLimit: 60, camera: 'third-person' } },
    { do: 'addMany', object: { prefab: 'target' }, count: 5, layout: 'line', center: [x, 0.95, z - 14], spacing: 4 },
    { do: 'addMany', object: { prefab: 'target', behaviors: [{ type: 'patrol', params: { axis: 'x', distance: 6, speed: 2 } }] }, count: 3, layout: 'line', center: [x - 3, 0.95, z - 24], spacing: 5 },
    { do: 'addMany', object: { prefab: 'wall', scale: [3, 1.2, 0.5], color: '#9a7b5a', pattern: 'wood' }, count: 4, layout: 'line', center: [x, 0.6, z - 6], spacing: 5 },
  ];
}

function castle(focus, color) {
  const c = color || '#b9b4aa';
  const [x, , z] = focus;
  const zc = z - 12;
  const s = 10;
  const actions = [];
  for (const [dx, dz, sx, sz] of [[0, -s, 2 * s, 1], [-s, 0, 1, 2 * s], [s, 0, 1, 2 * s]]) {
    actions.push({ do: 'add', object: { prefab: 'wall', position: [x + dx, 2, zc + dz], scale: [sx, 4, sz], color: c, pattern: 'bricks' } });
  }
  actions.push({ do: 'add', object: { prefab: 'wall', position: [x - s / 2 - 1.5, 2, zc + s], scale: [s - 3, 4, 1], color: c, pattern: 'bricks' } });
  actions.push({ do: 'add', object: { prefab: 'wall', position: [x + s / 2 + 1.5, 2, zc + s], scale: [s - 3, 4, 1], color: c, pattern: 'bricks' } });
  for (const [dx, dz] of [[-s, -s], [s, -s], [-s, s], [s, s]]) {
    actions.push({ do: 'add', object: { shape: 'cylinder', name: 'Tower', position: [x + dx, 3.5, zc + dz], scale: [3, 7, 3], color: c, pattern: 'stone' } });
    actions.push({ do: 'add', object: { shape: 'cone', name: 'Tower roof', position: [x + dx, 8.25, zc + dz], scale: [3.6, 2.5, 3.6], color: '#b5533c' } });
  }
  actions.push({ do: 'add', object: { prefab: 'star', position: [x, 1.2, zc] } });
  return actions;
}
