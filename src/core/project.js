// The project format: a whole game is one plain JSON object.
// Saving, loading, undo, the AI helper and publishing all work on this data.
import { SHAPES } from './shapes.js';
import { BEHAVIORS, defaultParams } from './behaviors.js';
import { ENV_PRESETS, environmentFromPreset } from './environment.js';
import { clamp, deepClone, newId, parseColor, toNumber, toVec3 } from './util.js';

export const PROJECT_FORMAT = 'xordor-project';
export const PROJECT_VERSION = 1;
export const MAX_OBJECTS = 3000;

export const LOOKS = {
  plastic: 'Plastic',
  matte: 'Matte',
  shiny: 'Shiny',
  metal: 'Metal',
  glass: 'Glass',
  glow: 'Glowing',
  cartoon: 'Cartoon',
};

export const PATTERNS = {
  none: 'Plain',
  checker: 'Checkers',
  bricks: 'Bricks',
  wood: 'Wood',
  crate: 'Crate',
  grass: 'Grass',
  stone: 'Stone',
  noise: 'Speckles',
  stripes: 'Stripes',
  dots: 'Dots',
  tiles: 'Tiles',
  arrows: 'Arrows',
};

export const PHYSICS = {
  static: 'Solid, stays in place',
  dynamic: 'Solid, falls and can be pushed',
  none: 'Ghost, nothing bumps into it',
};

export const CAMERAS = {
  'third-person': 'Behind the player',
  'first-person': 'Through the player\'s eyes',
  'top-down': 'From above',
};

export const WIN_CONDITIONS = {
  goal: 'Reach the finish line',
  'collect-all': 'Collect everything',
  score: 'Reach a target score',
  survive: 'Survive until the timer ends',
  none: 'No winning, just play',
};

export const DEFAULT_SETTINGS = {
  camera: 'third-person',
  winCondition: 'goal',
  targetScore: 10,
  lives: 3,
  timeLimit: 0,
  playerSpeed: 7,
  jumpPower: 9,
  gravity: 22,
  playerColor: '#4fb3ff',
  fallLimit: -25,
  startMessage: '',
};

const SETTING_LIMITS = {
  targetScore: [1, 100000],
  lives: [0, 99],
  timeLimit: [0, 3600],
  playerSpeed: [1, 60],
  jumpPower: [0, 60],
  gravity: [1, 100],
  fallLimit: [-1000, 1000],
};

/** Creates a new entity from a shape key plus optional overrides. */
export function createEntity(shape = 'box', overrides = {}) {
  const base = {
    id: newId(),
    name: SHAPES[shape]?.label || 'Object',
    shape,
    position: [0, 0.5, 0],
    rotation: [0, 0, 0],
    scale: [1, 1, 1],
    color: '#e0e0e0',
    look: 'plastic',
    pattern: 'none',
    physics: 'static',
    mass: 1,
    bounciness: 0.2,
    visible: true,
    behaviors: [],
    props: {},
  };
  return normalizeEntity({ ...base, ...deepClone(overrides) });
}

export function normalizeBehavior(raw) {
  if (!raw || typeof raw !== 'object') return null;
  const type = String(raw.type || raw.kind || raw.name || '').trim();
  const def = BEHAVIORS[type];
  if (!def) return null;
  const params = defaultParams(type);
  const src = raw.params && typeof raw.params === 'object' ? raw.params : raw;
  for (const [k, p] of Object.entries(def.params)) {
    if (!(k in src)) continue;
    const v = src[k];
    switch (p.type) {
      case 'number': params[k] = toNumber(v, p.default, p.min ?? -1e6, p.max ?? 1e6); break;
      case 'boolean': params[k] = typeof v === 'boolean' ? v : v === 'true' || v === 1; break;
      case 'select': params[k] = p.options.includes(v) ? v : p.default; break;
      case 'color': params[k] = parseColor(v, p.default); break;
      case 'vec3': params[k] = toVec3(v, p.default); break;
      case 'code': params[k] = typeof v === 'string' ? v.slice(0, 50000) : p.default; break;
      default: params[k] = v == null ? p.default : String(v).slice(0, 500);
    }
  }
  return { type, params };
}

export function normalizeEntity(raw) {
  const e = raw && typeof raw === 'object' ? raw : {};
  const shape = SHAPES[e.shape] ? e.shape : 'box';
  const shapeDef = SHAPES[shape];
  const props = {};
  if (shapeDef.props) {
    for (const [k, p] of Object.entries(shapeDef.props)) {
      const v = e.props?.[k];
      props[k] = p.type === 'number' ? toNumber(v, p.default, p.min ?? -1e6, p.max ?? 1e6) : v == null ? p.default : String(v).slice(0, 200);
    }
  }
  const behaviors = (Array.isArray(e.behaviors) ? e.behaviors : [])
    .map(normalizeBehavior)
    .filter(Boolean)
    .slice(0, 20);
  return {
    id: typeof e.id === 'string' && /^[\w-]{1,64}$/.test(e.id) ? e.id : newId(),
    name: (typeof e.name === 'string' && e.name.trim() ? e.name.trim() : shapeDef.label).slice(0, 60),
    shape,
    position: toVec3(e.position, [0, 0.5, 0]),
    rotation: toVec3(e.rotation, [0, 0, 0], -36000, 36000),
    scale: toVec3(e.scale, [1, 1, 1], 0.02, 2000),
    color: parseColor(e.color, '#e0e0e0'),
    look: LOOKS[e.look] ? e.look : 'plastic',
    pattern: PATTERNS[e.pattern] ? e.pattern : 'none',
    physics: PHYSICS[e.physics] ? e.physics : 'static',
    mass: toNumber(e.mass, 1, 0.01, 10000),
    bounciness: toNumber(e.bounciness, 0.2, 0, 1),
    visible: e.visible !== false,
    behaviors,
    props,
  };
}

export function normalizeSettings(raw = {}) {
  const s = { ...DEFAULT_SETTINGS };
  for (const k of Object.keys(DEFAULT_SETTINGS)) {
    if (!(k in (raw || {}))) continue;
    const v = raw[k];
    if (SETTING_LIMITS[k]) s[k] = toNumber(v, DEFAULT_SETTINGS[k], ...SETTING_LIMITS[k]);
    else if (k === 'camera') s[k] = CAMERAS[v] ? v : DEFAULT_SETTINGS.camera;
    else if (k === 'winCondition') s[k] = WIN_CONDITIONS[v] ? v : DEFAULT_SETTINGS.winCondition;
    else if (k === 'playerColor') s[k] = parseColor(v, DEFAULT_SETTINGS.playerColor);
    else if (k === 'startMessage') s[k] = typeof v === 'string' ? v.slice(0, 300) : '';
  }
  return s;
}

export function normalizeEnvironment(raw = {}) {
  const preset = raw && ENV_PRESETS[raw.preset] ? raw.preset : 'day';
  const base = environmentFromPreset(preset);
  const env = { ...base };
  const r = raw || {};
  for (const k of ['skyTop', 'skyBottom', 'sunColor', 'fogColor', 'groundColor']) {
    if (k in r) env[k] = parseColor(r[k], base[k]);
  }
  if ('sunIntensity' in r) env.sunIntensity = toNumber(r.sunIntensity, base.sunIntensity, 0, 20);
  if ('ambient' in r) env.ambient = toNumber(r.ambient, base.ambient, 0, 10);
  if ('fog' in r) env.fog = toNumber(r.fog, base.fog, 0, 1);
  if ('sunHeight' in r) env.sunHeight = toNumber(r.sunHeight, base.sunHeight, 2, 90);
  if ('stars' in r) env.stars = !!r.stars;
  if ('showGround' in r) env.showGround = r.showGround !== false;
  if ('groundSize' in r) env.groundSize = toNumber(r.groundSize, base.groundSize, 4, 2000);
  if ('groundPattern' in r) env.groundPattern = PATTERNS[r.groundPattern] ? r.groundPattern : base.groundPattern;
  return env;
}

export function normalizeProject(raw) {
  const p = raw && typeof raw === 'object' ? raw : {};
  const seen = new Set();
  const objects = (Array.isArray(p.objects) ? p.objects : []).slice(0, MAX_OBJECTS).map((o) => {
    const e = normalizeEntity(o);
    if (seen.has(e.id)) e.id = newId();
    seen.add(e.id);
    return e;
  });
  return {
    format: PROJECT_FORMAT,
    version: PROJECT_VERSION,
    name: (typeof p.name === 'string' && p.name.trim() ? p.name.trim() : 'My Game').slice(0, 80),
    settings: normalizeSettings(p.settings),
    environment: normalizeEnvironment(p.environment),
    objects,
  };
}

export function createProject(name = 'My Game') {
  return normalizeProject({
    name,
    objects: [createEntity('spawn', { name: 'Player start', position: [0, 0.06, 0], color: '#4fb3ff', physics: 'none' })],
  });
}

export function hasScripts(project) {
  return (project?.objects || []).some((o) => o.behaviors?.some((b) => b.type === 'script'));
}

export function stripScripts(project) {
  for (const o of project.objects) o.behaviors = o.behaviors.filter((b) => b.type !== 'script');
  return project;
}

/** How far below the centre the bottom of an entity is (to sit things on the ground). */
export function halfHeight(entity) {
  const def = SHAPES[entity.shape] || SHAPES.box;
  return (def.size[1] * entity.scale[1]) / 2;
}

export function clampCount(n, max = 200) {
  return clamp(Math.round(toNumber(n, 1, 1, max)), 1, max);
}
