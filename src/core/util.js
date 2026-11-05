// Small helpers shared by the engine, the editor and the AI layer.
// Everything in here is pure JavaScript so it can be unit tested in Node.
import { Color } from 'three';

export const clamp = (v, min, max) => Math.min(max, Math.max(min, v));
export const DEG = Math.PI / 180;

export function deepClone(value) {
  return value === undefined ? undefined : JSON.parse(JSON.stringify(value));
}

let idCounter = 0;
export function newId(prefix = 'obj') {
  idCounter = (idCounter + 1) % 1679616;
  const time = Date.now().toString(36).slice(-4);
  const rand = Math.random().toString(36).slice(2, 6);
  return `${prefix}_${time}${rand}${idCounter.toString(36)}`;
}

export function toNumber(value, fallback = 0, min = -Infinity, max = Infinity) {
  const n = typeof value === 'string' ? parseFloat(value) : value;
  if (typeof n !== 'number' || !Number.isFinite(n)) return fallback;
  return clamp(n, min, max);
}

export function toVec3(value, fallback = [0, 0, 0], min = -1e5, max = 1e5) {
  if (typeof value === 'number' && Number.isFinite(value)) {
    const v = clamp(value, min, max);
    return [v, v, v];
  }
  if (value && !Array.isArray(value) && typeof value === 'object') {
    value = [value.x, value.y, value.z];
  }
  if (!Array.isArray(value)) return [...fallback];
  return [0, 1, 2].map((i) => toNumber(value[i], fallback[i], min, max));
}

export function round(n, digits = 3) {
  const f = 10 ** digits;
  return Math.round(n * f) / f;
}

const colorScratch = new Color();

/** Turns "red", "#f00", "rgb(255,0,0)" or 0xff0000 into "#ff0000". */
export function parseColor(value, fallback = '#cccccc') {
  if (typeof value === 'number' && Number.isFinite(value)) {
    return '#' + colorScratch.setHex(value & 0xffffff).getHexString();
  }
  if (typeof value !== 'string') return fallback;
  const s = value.trim().toLowerCase();
  if (/^#?[0-9a-f]{6}$/.test(s)) return s.startsWith('#') ? s : '#' + s;
  if (/^#?[0-9a-f]{3}$/.test(s)) {
    const h = s.replace('#', '');
    return '#' + h.split('').map((c) => c + c).join('');
  }
  if (/^#?[0-9a-f]{8}$/.test(s)) return '#' + s.replace('#', '').slice(0, 6);
  const named = Color.NAMES[s.replace(/[\s_-]+/g, '')];
  if (named !== undefined) return '#' + colorScratch.setHex(named).getHexString();
  if (/^(rgb|hsl)a?\(/.test(s)) {
    try {
      return '#' + colorScratch.setStyle(s).getHexString();
    } catch {
      return fallback;
    }
  }
  return fallback;
}

/** Deterministic pseudo random numbers (mulberry32), handy for layouts. */
export function seededRandom(seed = 1) {
  let a = seed >>> 0;
  return () => {
    a = (a + 0x6d2b79f5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

export function debounce(fn, ms) {
  let t = null;
  const wrapped = (...args) => {
    clearTimeout(t);
    t = setTimeout(() => fn(...args), ms);
  };
  wrapped.flush = (...args) => {
    clearTimeout(t);
    fn(...args);
  };
  return wrapped;
}

export function safeStorage() {
  try {
    const k = '__xordor_probe__';
    window.localStorage.setItem(k, '1');
    window.localStorage.removeItem(k);
    return window.localStorage;
  } catch {
    const mem = new Map();
    return {
      getItem: (k) => (mem.has(k) ? mem.get(k) : null),
      setItem: (k, v) => mem.set(k, String(v)),
      removeItem: (k) => mem.delete(k),
    };
  }
}
