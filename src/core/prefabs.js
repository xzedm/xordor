// The "Add" palette: ready-to-use things with sensible colors and rules.
// A prefab is just a shape plus default settings, so the AI can use them too.

const b = (type, params = {}) => ({ type, params });

export const PREFAB_GROUPS = [
  {
    key: 'game',
    label: 'Game pieces',
    items: [
      { key: 'start', label: 'Player start', shape: 'spawn', color: '#4fb3ff', physics: 'none', unique: true },
      { key: 'coin', label: 'Coin', shape: 'coin', color: '#ffc531', look: 'metal', physics: 'none', behaviors: [b('collectible', { points: 1 }), b('spin', { speed: 140 })], lift: 0.6 },
      { key: 'star', label: 'Star', shape: 'star', color: '#ffe14d', look: 'glow', physics: 'none', behaviors: [b('collectible', { points: 5 }), b('spin', { speed: 90 }), b('bob', { height: 0.25, speed: 0.7 })], lift: 0.8 },
      { key: 'gem', label: 'Gem', shape: 'gem', color: '#38e1c6', look: 'shiny', physics: 'none', behaviors: [b('collectible', { points: 3 }), b('spin', { speed: 60 }), b('bob', { height: 0.2, speed: 0.5 })], lift: 0.6 },
      { key: 'heart', label: 'Extra life', shape: 'heart', color: '#ff4d6d', look: 'shiny', physics: 'none', behaviors: [b('collectible', { points: 0, lives: 1 }), b('spin', { speed: 90 })], lift: 0.6 },
      { key: 'key', label: 'Key', shape: 'key', color: '#ffcf33', look: 'metal', physics: 'none', behaviors: [b('collectible', { points: 0, item: 'key' }), b('spin', { speed: 120 }), b('bob', { height: 0.15, speed: 0.8 })], lift: 0.6 },
      { key: 'door', label: 'Locked door', shape: 'box', scale: [2, 3, 0.3], color: '#9a6a3c', pattern: 'wood', physics: 'static', behaviors: [b('door', { needs: 'key' })] },
      { key: 'goal', label: 'Finish flag', shape: 'flag', color: '#ff4d4d', physics: 'none', behaviors: [b('goal')] },
      { key: 'checkpoint', label: 'Checkpoint', shape: 'checkpoint', color: '#36c46b', physics: 'none', behaviors: [b('checkpoint')] },
      { key: 'enemy', label: 'Enemy', shape: 'enemy', color: '#8e44ec', physics: 'none', behaviors: [b('chase', { speed: 3, range: 12 }), b('hazard', { damage: 1 }), b('health', { hp: 3, points: 5 })], lift: 0.1 },
      { key: 'spikes', label: 'Spikes', shape: 'spikes', color: '#c9ced6', look: 'metal', physics: 'static', behaviors: [b('hazard', { damage: 1 })] },
      { key: 'lava', label: 'Lava', shape: 'box', scale: [4, 0.3, 4], color: '#ff5a1f', look: 'glow', pattern: 'noise', physics: 'static', behaviors: [b('hazard', { damage: 1 })] },
      { key: 'bouncepad', label: 'Bounce pad', shape: 'bouncepad', color: '#ff3fa4', physics: 'static', behaviors: [b('bounce', { power: 16 })] },
      { key: 'speedpad', label: 'Speed pad', shape: 'speedpad', scale: [1.5, 1, 1.5], color: '#3fd0ff', physics: 'static', behaviors: [b('speed', { multiplier: 2, seconds: 2.5 })] },
      { key: 'portal', label: 'Portal', shape: 'portal', scale: [1.5, 1.5, 1.5], color: '#a45cff', physics: 'none', behaviors: [b('teleport', { target: [0, 2, 0] }), b('spin', { axis: 'z', speed: 30 })] },
      { key: 'mover', label: 'Moving platform', shape: 'box', scale: [3, 0.4, 3], color: '#f2a93b', pattern: 'tiles', physics: 'static', behaviors: [b('patrol', { axis: 'x', distance: 5, speed: 2 })], lift: 1 },
      { key: 'spinner', label: 'Spinning bar', shape: 'box', scale: [7, 0.4, 0.4], color: '#e94b4b', pattern: 'stripes', physics: 'static', behaviors: [b('spin', { speed: 70 })], lift: 0.3 },
      { key: 'crumble', label: 'Falling platform', shape: 'box', scale: [2.5, 0.4, 2.5], color: '#c7a27a', pattern: 'crate', physics: 'static', behaviors: [b('vanish', { delay: 0.6, comeBack: 3 })], lift: 1 },
      { key: 'target', label: 'Target', shape: 'target', color: '#e63946', physics: 'static', behaviors: [b('health', { hp: 1, points: 1, effect: 'pop' })] },
      { key: 'turret', label: 'Turret', shape: 'turret', color: '#5b6378', physics: 'static', behaviors: [b('shooter', { every: 1.6, range: 24, speed: 16 }), b('health', { hp: 4, points: 10 })] },
      { key: 'car', label: 'Car', shape: 'car', color: '#e63946', look: 'shiny', physics: 'dynamic', mass: 150, behaviors: [b('vehicle', { speed: 26, startInside: false })] },
      { key: 'dropper', label: 'Ball dropper', shape: 'cloud', scale: [2, 2, 2], color: '#5b6378', physics: 'none', behaviors: [b('spawner', { every: 1.5, size: 0.7, color: '#ff7043', harmful: true }), b('bob', { height: 0.3, speed: 0.4 })], lift: 6 },
    ],
  },
  {
    key: 'build',
    label: 'Building',
    items: [
      { key: 'platform', label: 'Platform', shape: 'box', scale: [4, 0.5, 4], color: '#d9dde4', pattern: 'tiles', physics: 'static' },
      { key: 'wall', label: 'Wall', shape: 'box', scale: [6, 3, 0.5], color: '#c96f4a', pattern: 'bricks', physics: 'static' },
      { key: 'pillar', label: 'Pillar', shape: 'cylinder', scale: [0.8, 4, 0.8], color: '#e6e1d6', pattern: 'stone', physics: 'static' },
      { key: 'stairs', label: 'Stairs', shape: 'stairs', scale: [2, 2, 3], color: '#b8bec9', pattern: 'stone', physics: 'static' },
      { key: 'ramp', label: 'Ramp', shape: 'ramp', scale: [2, 1.5, 4], color: '#7fb3d5', physics: 'static' },
      { key: 'house', label: 'House', shape: 'house', scale: [1.5, 1.5, 1.5], color: '#f1d9a8', physics: 'static' },
      { key: 'crate', label: 'Crate', shape: 'box', color: '#b9874f', pattern: 'crate', physics: 'dynamic', mass: 1 },
      { key: 'barrel', label: 'Barrel', shape: 'cylinder', scale: [0.8, 1.1, 0.8], color: '#c0392b', pattern: 'stripes', physics: 'dynamic', mass: 1.5 },
      { key: 'bouncyball', label: 'Bouncy ball', shape: 'sphere', color: '#ff9f1c', look: 'shiny', physics: 'dynamic', mass: 0.5, bounciness: 0.95 },
      { key: 'sign', label: 'Sign', shape: 'sign', color: '#f7e7c1', physics: 'static', props: { text: 'Welcome!' } },
      { key: 'water', label: 'Water', shape: 'box', scale: [6, 0.2, 6], color: '#2f9ee8', look: 'glass', physics: 'none' },
    ],
  },
  {
    key: 'nature',
    label: 'Nature',
    items: [
      { key: 'tree', label: 'Tree', shape: 'tree', scale: [1.4, 1.4, 1.4], color: '#4caf50', physics: 'static' },
      { key: 'pine', label: 'Pine tree', shape: 'pine', scale: [1.4, 1.4, 1.4], color: '#2e7d4f', physics: 'static' },
      { key: 'rock', label: 'Rock', shape: 'rock', scale: [1.4, 1.4, 1.4], color: '#8d8f96', look: 'matte', physics: 'static', randomSeed: true },
      { key: 'bush', label: 'Bush', shape: 'bush', color: '#5cb85c', look: 'matte', physics: 'static' },
      { key: 'cloud', label: 'Cloud', shape: 'cloud', scale: [2.5, 2.5, 2.5], color: '#ffffff', look: 'matte', physics: 'none', behaviors: [b('bob', { height: 0.4, speed: 0.15 })], lift: 9 },
    ],
  },
  {
    key: 'shapes',
    label: 'Basic shapes',
    items: [
      { key: 'cube', label: 'Cube', shape: 'box', color: '#5aa9ff', physics: 'static' },
      { key: 'ball', label: 'Ball', shape: 'sphere', color: '#ff6b6b', physics: 'static' },
      { key: 'cylinder', label: 'Cylinder', shape: 'cylinder', color: '#ffd166', physics: 'static' },
      { key: 'cone', label: 'Cone', shape: 'cone', color: '#06d6a0', physics: 'static' },
      { key: 'pyramid', label: 'Pyramid', shape: 'pyramid', color: '#f4a261', physics: 'static' },
      { key: 'capsule', label: 'Capsule', shape: 'capsule', color: '#c77dff', physics: 'static' },
      { key: 'ring', label: 'Ring', shape: 'torus', color: '#ff8fab', physics: 'static' },
    ],
  },
  {
    key: 'lights',
    label: 'Lights',
    items: [
      { key: 'lamp', label: 'Light', shape: 'light', color: '#ffd28a', look: 'glow', physics: 'none', props: { brightness: 3, range: 14 }, lift: 2.5 },
    ],
  },
];

export const PREFABS = Object.fromEntries(PREFAB_GROUPS.flatMap((g) => g.items.map((i) => [i.key, i])));

/** Converts a prefab into entity overrides for createEntity(). */
export function prefabToEntity(key) {
  const p = PREFABS[key];
  if (!p) return null;
  const { key: _k, label, unique, lift, randomSeed, ...rest } = p;
  const overrides = { name: label, ...JSON.parse(JSON.stringify(rest)) };
  if (randomSeed) overrides.props = { ...(overrides.props || {}), seed: 1 + Math.floor(Math.random() * 900) };
  return { overrides, lift: lift || 0, unique: !!unique };
}

export function describePrefabs() {
  return PREFAB_GROUPS.map((g) => `${g.label}: ${g.items.map((i) => i.key).join(', ')}`).join('\n');
}
