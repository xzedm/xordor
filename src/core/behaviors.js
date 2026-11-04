// Behaviors are the "rules" you can attach to objects with no coding:
// spin, float, chase the player, collect for points, hurt the player, etc.
// This file only describes them (names, settings, defaults). The code that
// makes them work while playing lives in src/engine/behaviorRuntime.js.

export const BEHAVIORS = {
  spin: {
    label: 'Spin',
    group: 'Movement',
    desc: 'Keeps turning around.',
    params: {
      axis: { type: 'select', label: 'Turn around', options: ['y', 'x', 'z'], default: 'y' },
      speed: { type: 'number', label: 'Speed (degrees per second)', default: 90, min: -2000, max: 2000, step: 10 },
    },
  },
  bob: {
    label: 'Float up and down',
    group: 'Movement',
    desc: 'Gently floats up and down.',
    params: {
      height: { type: 'number', label: 'How high (meters)', default: 0.4, min: 0, max: 50, step: 0.1 },
      speed: { type: 'number', label: 'Bobs per second', default: 0.6, min: 0.01, max: 10, step: 0.1 },
    },
  },
  patrol: {
    label: 'Move back and forth',
    group: 'Movement',
    desc: 'Slides back and forth. Great for moving platforms.',
    params: {
      axis: { type: 'select', label: 'Direction', options: ['x', 'z', 'y'], default: 'x' },
      distance: { type: 'number', label: 'Distance (meters)', default: 4, min: 0, max: 500, step: 0.5 },
      speed: { type: 'number', label: 'Speed (meters per second)', default: 2, min: 0.01, max: 100, step: 0.5 },
    },
  },
  chase: {
    label: 'Chase the player',
    group: 'Movement',
    desc: 'Follows the player when they come close.',
    params: {
      speed: { type: 'number', label: 'Speed (meters per second)', default: 3, min: 0, max: 100, step: 0.5 },
      range: { type: 'number', label: 'Notices the player within (meters)', default: 12, min: 0, max: 1000, step: 1 },
    },
  },
  collectible: {
    label: 'Collect for points',
    group: 'Game rules',
    desc: 'Disappears when touched and gives points, lives or an item.',
    params: {
      points: { type: 'number', label: 'Points', default: 1, min: -1000, max: 1000, step: 1 },
      lives: { type: 'number', label: 'Extra lives', default: 0, min: 0, max: 99, step: 1 },
      item: { type: 'text', label: 'Item name (for doors)', default: '' },
    },
  },
  hazard: {
    label: 'Hurts the player',
    group: 'Game rules',
    desc: 'Touching it costs a life and sends the player back to the last checkpoint.',
    params: {
      damage: { type: 'number', label: 'Lives lost', default: 1, min: 0, max: 99, step: 1 },
    },
  },
  goal: {
    label: 'Finish line',
    group: 'Game rules',
    desc: 'Touching it wins the game (when the win rule is "Reach the finish").',
    params: {},
  },
  checkpoint: {
    label: 'Checkpoint',
    group: 'Game rules',
    desc: 'The player comes back here after getting hurt.',
    params: {},
  },
  bounce: {
    label: 'Bounce pad',
    group: 'Game rules',
    desc: 'Launches the player up into the air.',
    params: {
      power: { type: 'number', label: 'Bounce power', default: 16, min: 0, max: 200, step: 1 },
    },
  },
  speed: {
    label: 'Speed boost',
    group: 'Game rules',
    desc: 'Makes the player run faster for a moment.',
    params: {
      multiplier: { type: 'number', label: 'Times faster', default: 2, min: 0.1, max: 10, step: 0.1 },
      seconds: { type: 'number', label: 'For how long (seconds)', default: 2.5, min: 0.1, max: 120, step: 0.5 },
    },
  },
  teleport: {
    label: 'Teleporter',
    group: 'Game rules',
    desc: 'Sends the player to another spot.',
    params: {
      target: { type: 'vec3', label: 'Send player to (x, y, z)', default: [0, 2, 0] },
    },
  },
  vanish: {
    label: 'Vanish when touched',
    group: 'Game rules',
    desc: 'Disappears shortly after the player touches it, like a crumbling platform.',
    params: {
      delay: { type: 'number', label: 'Wait before vanishing (seconds)', default: 0.6, min: 0, max: 60, step: 0.1 },
      comeBack: { type: 'number', label: 'Comes back after (seconds, 0 = never)', default: 3, min: 0, max: 600, step: 0.5 },
    },
  },
  message: {
    label: 'Show a message',
    group: 'Game rules',
    desc: 'Shows text on screen when the player touches it.',
    params: {
      text: { type: 'text', label: 'Message', default: 'Hello there!' },
      once: { type: 'boolean', label: 'Only the first time', default: true },
    },
  },
  door: {
    label: 'Locked door',
    group: 'Game rules',
    desc: 'Opens (disappears) when the player touches it while holding the right item.',
    params: {
      needs: { type: 'text', label: 'Item needed', default: 'key' },
    },
  },
  health: {
    label: 'Can be destroyed',
    group: 'Combat',
    desc: 'Takes damage when the player shoots it, and disappears when its health runs out.',
    params: {
      hp: { type: 'number', label: 'Health (hits with the blaster)', default: 3, min: 0.1, max: 10000, step: 1 },
      points: { type: 'number', label: 'Points for destroying it', default: 5, min: -10000, max: 10000, step: 1 },
      effect: { type: 'select', label: 'When destroyed', options: ['explode', 'pop', 'vanish'], default: 'explode' },
    },
  },
  shooter: {
    label: 'Shoots at the player',
    group: 'Combat',
    desc: 'Fires at the player when they come close.',
    params: {
      every: { type: 'number', label: 'Seconds between shots', default: 1.5, min: 0.1, max: 60, step: 0.1 },
      range: { type: 'number', label: 'Range (meters)', default: 22, min: 1, max: 500, step: 1 },
      speed: { type: 'number', label: 'Shot speed (m/s)', default: 18, min: 1, max: 200, step: 1 },
      damage: { type: 'number', label: 'Lives lost per hit', default: 1, min: 0, max: 99, step: 1 },
      color: { type: 'color', label: 'Shot color', default: '#ff4d4d' },
    },
  },
  vehicle: {
    label: 'Drivable vehicle',
    group: 'Driving',
    desc: 'The player can get in and drive it. Press F next to it to get in or out.',
    params: {
      speed: { type: 'number', label: 'Top speed (m/s)', default: 26, min: 1, max: 150, step: 1 },
      grip: { type: 'number', label: 'Tire grip', default: 1.6, min: 0.2, max: 10, step: 0.1 },
      startInside: { type: 'boolean', label: 'The player starts in this vehicle', default: false },
    },
  },
  spawner: {
    label: 'Drop or throw balls',
    group: 'Game rules',
    desc: 'Creates balls every few seconds. They can be thrown at the player.',
    params: {
      every: { type: 'number', label: 'Every (seconds)', default: 2, min: 0.1, max: 600, step: 0.1 },
      throwSpeed: { type: 'number', label: 'Throw at player speed (0 = just drop)', default: 0, min: 0, max: 200, step: 1 },
      size: { type: 'number', label: 'Ball size (meters)', default: 0.6, min: 0.05, max: 20, step: 0.1 },
      color: { type: 'color', label: 'Ball color', default: '#ff7043' },
      harmful: { type: 'boolean', label: 'Balls hurt the player', default: false },
      max: { type: 'number', label: 'Most balls at once', default: 15, min: 1, max: 200, step: 1 },
    },
  },
  script: {
    label: 'Custom code',
    group: 'Advanced',
    desc: 'JavaScript that runs while playing. The AI helper can write this for you.',
    params: {
      code: {
        type: 'code',
        label: 'Code',
        default: '// Runs every frame while playing.\nfunction onUpdate(dt) {\n  self.rotation.y += dt;\n}\n\n// Runs when the player touches this object.\nfunction onTouch() {\n  game.message("You found me!");\n}\n',
      },
    },
  },
};

export const BEHAVIOR_KEYS = Object.keys(BEHAVIORS);

/** Behaviors that move an object around (so the physics body becomes "kinematic"). */
export const MOTION_BEHAVIORS = new Set(['spin', 'bob', 'patrol', 'chase', 'script']);

export function defaultParams(type) {
  const def = BEHAVIORS[type];
  if (!def) return {};
  const out = {};
  for (const [k, p] of Object.entries(def.params)) out[k] = Array.isArray(p.default) ? [...p.default] : p.default;
  return out;
}

export function describeBehaviors() {
  return BEHAVIOR_KEYS.map((k) => {
    const b = BEHAVIORS[k];
    const params = Object.entries(b.params)
      .map(([pk, p]) => `${pk}: ${p.type}${p.options ? ` (${p.options.join('|')})` : ''} = ${JSON.stringify(p.default)}`)
      .join('; ');
    return `- ${k}: ${b.desc}${params ? ` Params: ${params}` : ''}`;
  }).join('\n');
}
