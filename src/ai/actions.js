// Applies "actions" (from the AI helper, or the built-in helper) to a project.
// This is pure data work: it never touches the 3D scene, so it is easy to test
// and every AI change can be undone in one step.
import {
  clampCount, createEntity, halfHeight, MAX_OBJECTS, normalizeBehavior, normalizeEntity,
  normalizeEnvironment, normalizeSettings,
} from '../core/project.js';
import { PREFABS, prefabToEntity } from '../core/prefabs.js';
import { SHAPES } from '../core/shapes.js';
import { ENV_PRESETS, environmentFromPreset } from '../core/environment.js';
import { deepClone, seededRandom, toNumber, toVec3 } from '../core/util.js';

const SHAPE_ALIASES = {
  cube: 'box', block: 'box', square: 'box', ball: 'sphere', orb: 'sphere', tube: 'cylinder', ring: 'torus',
  donut: 'torus', wedge: 'ramp', slope: 'ramp', diamond: 'gem', crystal: 'gem', finish: 'flag', goal: 'flag',
  monster: 'enemy', villain: 'enemy', spike: 'spikes', trampoline: 'bouncepad', start: 'spawn', lamp: 'light',
  boulder: 'rock', stone: 'rock', building: 'house', stair: 'stairs', plant: 'bush',
};

function resolveShape(name) {
  if (!name) return null;
  const k = String(name).toLowerCase().trim();
  if (SHAPES[k]) return k;
  if (SHAPE_ALIASES[k]) return SHAPE_ALIASES[k];
  const singular = k.replace(/s$/, '');
  if (SHAPES[singular]) return singular;
  return SHAPE_ALIASES[singular] || null;
}

/** Turns an AI "object" spec (shape or prefab + overrides) into a full entity. */
export function specToEntity(spec = {}) {
  const s = { ...spec };
  delete s.id;
  let base = {};
  let lift = 0;
  const shapeAsPrefab = s.shape && PREFABS[s.shape] && (!SHAPES[s.shape] || s.behaviors === undefined) ? s.shape : null;
  const prefabKey = s.prefab && PREFABS[s.prefab] ? s.prefab : shapeAsPrefab || (!s.shape && PREFABS[s.type] ? s.type : null);
  if (prefabKey) {
    const p = prefabToEntity(prefabKey);
    base = p.overrides;
    lift = p.lift;
  }
  const shape = (prefabKey === shapeAsPrefab && base.shape) || resolveShape(s.shape) || base.shape || resolveShape(s.type) || 'box';
  const merged = { ...base, ...s, shape };
  delete merged.prefab;
  delete merged.type;
  delete merged.replaceBehaviors;
  delete merged.near;
  delete merged.scaleBy;
  delete merged.onGround;
  if (s.behaviors !== undefined) {
    const list = (Array.isArray(s.behaviors) ? s.behaviors : [s.behaviors])
      .map((b) => (typeof b === 'string' ? { type: b } : b))
      .filter((b) => b && typeof b === 'object');
    if (base.behaviors && !s.replaceBehaviors) {
      // Extra behaviors are added on top of the prefab's own rules.
      const byType = new Map(base.behaviors.map((b) => [b.type, b]));
      for (const b of list) {
        const type = b.type || b.kind;
        const { type: _t, kind: _k, params, ...flat } = b;
        byType.set(type, { type, params: { ...(byType.get(type)?.params || {}), ...(params || flat) } });
      }
      merged.behaviors = [...byType.values()];
    } else merged.behaviors = list;
  }
  if (typeof merged.scale === 'number') merged.scale = [merged.scale, merged.scale, merged.scale];
  if (merged.size !== undefined && merged.scale === undefined) {
    // "size" is in meters, convert to scale.
    const want = toVec3(merged.size, SHAPES[shape].size, 0.02, 4000);
    merged.scale = want.map((w, i) => w / SHAPES[shape].size[i]);
  }
  delete merged.size;
  const entity = createEntity(shape, merged);
  if (spec.scaleBy !== undefined) {
    const k = toVec3(spec.scaleBy, [1, 1, 1], 0.01, 100);
    entity.scale = entity.scale.map((v, i) => Math.min(2000, Math.max(0.02, v * k[i])));
  }
  if (!spec.position) {
    const near = spec.near ? toVec3(spec.near) : [0, 0, 0];
    entity.position = [near[0], halfHeight(entity) + lift, near[2]];
  } else if (spec.onGround) {
    entity.position[1] += halfHeight(entity);
  }
  return entity;
}

function matchTargets(project, a, selectedId) {
  const objs = project.objects;
  if (a.all === true || a.target === 'all') {
    return objs.filter((o) => (!a.shape || o.shape === resolveShape(a.shape)) && (!a.hasBehavior || o.behaviors.some((b) => b.type === a.hasBehavior)));
  }
  const ids = new Set([...(Array.isArray(a.ids) ? a.ids : []), ...(a.id ? [a.id] : [])]);
  if (a.target === 'selected' || a.id === 'selected') {
    ids.delete('selected');
    if (selectedId) ids.add(selectedId);
  }
  const found = objs.filter((o) => ids.has(o.id));
  if (a.name) {
    const n = String(a.name).toLowerCase();
    const exact = objs.filter((o) => o.name.toLowerCase() === n);
    const loose = exact.length ? exact : objs.filter((o) => o.name.toLowerCase().includes(n));
    for (const o of loose) if (!found.includes(o)) found.push(o);
  }
  if (a.shape && !a.id && !a.ids && !a.name) {
    const shape = resolveShape(a.shape);
    for (const o of objs) if (o.shape === shape && !found.includes(o)) found.push(o);
  }
  if (a.hasBehavior && !a.id && !a.ids && !a.name) {
    for (const o of objs) if (o.behaviors.some((b) => b.type === a.hasBehavior) && !found.includes(o)) found.push(o);
  }
  return found;
}

function layoutPositions(a, count) {
  const center = toVec3(a.center ?? a.position ?? [0, 0, 0]);
  const layout = String(a.layout || 'line').toLowerCase();
  const spacing = toNumber(a.spacing, 2, 0.05, 1000);
  const out = [];
  const rand = seededRandom(toNumber(a.seed, 7, 0, 1e9));
  switch (layout) {
    case 'circle':
    case 'ring': {
      const radius = toNumber(a.radius, Math.max(3, (count * spacing) / (2 * Math.PI)), 0.1, 2000);
      for (let i = 0; i < count; i++) {
        const t = (i / count) * Math.PI * 2;
        out.push([center[0] + Math.cos(t) * radius, center[1], center[2] + Math.sin(t) * radius]);
      }
      break;
    }
    case 'grid': {
      const cols = Math.max(1, Math.round(toNumber(a.columns, Math.ceil(Math.sqrt(count)), 1, 500)));
      const rows = Math.ceil(count / cols);
      for (let i = 0; i < count; i++) {
        const c = i % cols;
        const r = Math.floor(i / cols);
        out.push([center[0] + (c - (cols - 1) / 2) * spacing, center[1], center[2] + (r - (rows - 1) / 2) * spacing]);
      }
      break;
    }
    case 'random':
    case 'scatter': {
      const area = toVec3(a.area ?? [30, 0, 30], [30, 0, 30], 0, 4000);
      for (let i = 0; i < count; i++) out.push([center[0] + (rand() - 0.5) * area[0], center[1] + (rand() - 0.5) * area[1], center[2] + (rand() - 0.5) * area[2]]);
      break;
    }
    case 'stairs':
    case 'staircase': {
      const rise = toNumber(a.rise, 1, -100, 100);
      const dir = toVec3(a.direction ?? [0, 0, -1]);
      for (let i = 0; i < count; i++) out.push([center[0] + dir[0] * spacing * i, center[1] + rise * i, center[2] + dir[2] * spacing * i]);
      break;
    }
    case 'tower':
    case 'stack': {
      for (let i = 0; i < count; i++) out.push([center[0], center[1] + spacing * i, center[2]]);
      break;
    }
    case 'spiral': {
      const radius = toNumber(a.radius, 6, 0.1, 2000);
      const rise = toNumber(a.rise, 0.8, -100, 100);
      for (let i = 0; i < count; i++) {
        const t = (i / Math.max(1, count)) * Math.PI * 2 * toNumber(a.turns, 1.5, 0.1, 50);
        out.push([center[0] + Math.cos(t) * radius, center[1] + rise * i, center[2] + Math.sin(t) * radius]);
      }
      break;
    }
    default: {
      const dir = toVec3(a.direction ?? [1, 0, 0]);
      const len = Math.hypot(...dir) || 1;
      for (let i = 0; i < count; i++) {
        const k = (i - (count - 1) / 2) * spacing;
        out.push([center[0] + (dir[0] / len) * k, center[1] + (dir[1] / len) * k, center[2] + (dir[2] / len) * k]);
      }
    }
  }
  return out;
}

function applySet(entity, set) {
  const next = { ...entity };
  for (const [k, v] of Object.entries(set || {})) {
    if (k === 'id') continue;
    if (k === 'behaviors') next.behaviors = (Array.isArray(v) ? v : [v]).map((b) => (typeof b === 'string' ? { type: b } : b));
    else if (k === 'props') next.props = { ...next.props, ...v };
    else if (k === 'scale' && typeof v === 'number') next.scale = [v, v, v];
    else if (k === 'size') {
      const def = SHAPES[next.shape];
      next.scale = toVec3(v, def.size, 0.02, 4000).map((w, i) => w / def.size[i]);
    } else if (k === 'shape') next.shape = resolveShape(v) || next.shape;
    else next[k] = v;
  }
  return next;
}

/**
 * Applies a list of actions. Returns { project, summary, warnings, select }.
 * The input project is not modified.
 */
export function applyActions(project, actions, { selectedId = null } = {}) {
  const p = deepClone(project);
  const summary = { added: 0, changed: 0, removed: 0, world: false, rules: false };
  const warnings = [];
  let select = null;
  const list = Array.isArray(actions) ? actions.slice(0, 400) : [];

  for (const raw of list) {
    if (!raw || typeof raw !== 'object') continue;
    const kind = String(raw.do || raw.action || raw.type || raw.op || '').toLowerCase();
    try {
      switch (kind) {
        case 'add':
        case 'create': {
          if (p.objects.length >= MAX_OBJECTS) { warnings.push('The world is full, so some things were not added.'); break; }
          const spec = raw.object || raw.obj || raw.item || raw;
          const e = specToEntity(spec);
          if (e.shape === 'spawn') p.objects = p.objects.filter((o) => o.shape !== 'spawn');
          p.objects.push(e);
          summary.added++;
          select = e.id;
          break;
        }
        case 'addmany':
        case 'add_many':
        case 'repeat': {
          const spec = raw.object || raw.obj || raw.item || {};
          const count = clampCount(raw.count ?? 5, 300);
          const positions = layoutPositions(raw, count);
          const sample = specToEntity({ ...spec, position: undefined });
          const autoY = sample.position[1]; // resting on the ground (plus floating items' lift)
          const hasCenter = raw.center !== undefined || raw.position !== undefined;
          const yOffset = !hasCenter || raw.onGround ? autoY : 0;
          for (let i = 0; i < count; i++) {
            if (p.objects.length >= MAX_OBJECTS) { warnings.push('The world is full, so some things were not added.'); break; }
            const pos = positions[i];
            const e = specToEntity({ ...spec, position: [pos[0], pos[1] + yOffset, pos[2]] });
            if (count > 1) e.name = `${e.name} ${i + 1}`;
            if (raw.faceCenter && (raw.layout === 'circle' || raw.layout === 'ring')) {
              const c = toVec3(raw.center ?? [0, 0, 0]);
              e.rotation[1] = (Math.atan2(c[0] - pos[0], c[2] - pos[2]) * 180) / Math.PI;
            }
            p.objects.push(e);
            summary.added++;
          }
          break;
        }
        case 'update':
        case 'change':
        case 'modify':
        case 'edit': {
          const targets = matchTargets(p, raw, selectedId);
          if (!targets.length) { warnings.push(`Couldn't find ${raw.name || raw.id || 'that object'} to change.`); break; }
          const set = raw.set || raw.changes || raw.props || raw.object || {};
          for (const t of targets) {
            const idx = p.objects.indexOf(t);
            let next = applySet(t, set);
            if (raw.move) next.position = next.position.map((v, i) => v + toVec3(raw.move)[i]);
            if (raw.rotateBy) next.rotation = next.rotation.map((v, i) => v + toVec3(raw.rotateBy)[i]);
            if (raw.scaleBy !== undefined) next.scale = next.scale.map((v, i) => v * toVec3(raw.scaleBy, [1, 1, 1])[i]);
            if (raw.addBehavior) {
              const adds = Array.isArray(raw.addBehavior) ? raw.addBehavior : [raw.addBehavior];
              for (const b of adds) {
                const nb = normalizeBehavior(typeof b === 'string' ? { type: b } : b);
                if (nb) next.behaviors = [...next.behaviors.filter((x) => x.type !== nb.type), nb];
              }
            }
            if (raw.removeBehavior) {
              const rm = new Set(Array.isArray(raw.removeBehavior) ? raw.removeBehavior : [raw.removeBehavior]);
              next.behaviors = next.behaviors.filter((x) => !rm.has(x.type));
            }
            next = normalizeEntity(next);
            next.id = t.id;
            p.objects[idx] = next;
            summary.changed++;
          }
          if (targets.length === 1) select = targets[0].id;
          break;
        }
        case 'delete':
        case 'remove': {
          const targets = new Set(matchTargets(p, raw, selectedId));
          if (!targets.size) { warnings.push(`Couldn't find ${raw.name || raw.id || 'that object'} to remove.`); break; }
          p.objects = p.objects.filter((o) => !targets.has(o));
          summary.removed += targets.size;
          break;
        }
        case 'clear': {
          const keepStart = raw.keepStart !== false;
          const before = p.objects.length;
          p.objects = p.objects.filter((o) => keepStart && o.shape === 'spawn');
          summary.removed += before - p.objects.length;
          break;
        }
        case 'environment':
        case 'world':
        case 'sky': {
          const set = raw.set || raw;
          let env = p.environment;
          if (set.preset && ENV_PRESETS[set.preset]) env = { ...environmentFromPreset(set.preset), showGround: env.showGround, groundSize: env.groundSize };
          const { preset, do: _d, action: _a, type: _t, set: _s, ...rest } = set;
          p.environment = normalizeEnvironment({ ...env, ...rest, preset: env.preset });
          summary.world = true;
          break;
        }
        case 'settings':
        case 'rules':
        case 'game': {
          const set = raw.set || raw;
          p.settings = normalizeSettings({ ...p.settings, ...set });
          summary.rules = true;
          break;
        }
        case 'rename':
        case 'name': {
          const name = raw.name || raw.title;
          if (typeof name === 'string' && name.trim()) { p.name = name.trim().slice(0, 80); summary.rules = true; }
          break;
        }
        case 'gamescript':
        case 'game_script': {
          if (typeof raw.code === 'string') { p.gameScript = raw.code.slice(0, 200000); summary.rules = true; }
          break;
        }
        case 'select': {
          const targets = matchTargets(p, raw, selectedId);
          if (targets[0]) select = targets[0].id;
          break;
        }
        default:
          if (kind) warnings.push(`Skipped an unknown step "${kind}".`);
      }
    } catch (err) {
      warnings.push(`One step failed: ${err.message}`);
    }
  }
  return { project: p, summary, warnings, select };
}

export function describeSummary(summary) {
  const parts = [];
  if (summary.added) parts.push(`added ${summary.added}`);
  if (summary.changed) parts.push(`changed ${summary.changed}`);
  if (summary.removed) parts.push(`removed ${summary.removed}`);
  const things = parts.length ? `${parts.join(', ')} object${summary.added + summary.changed + summary.removed === 1 ? '' : 's'}` : '';
  const extra = [summary.world ? 'updated the sky and world' : '', summary.rules ? 'updated the game rules' : ''].filter(Boolean);
  const all = [things, ...extra].filter(Boolean);
  if (!all.length) return '';
  const text = all.join(', ');
  return text.charAt(0).toUpperCase() + text.slice(1) + '.';
}

export function hasChanges(summary) {
  return !!(summary.added || summary.changed || summary.removed || summary.world || summary.rules);
}
