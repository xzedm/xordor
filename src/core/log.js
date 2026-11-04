// A small shared log. Game scripts, the engine, the editor and the AI helper
// write here, and the Output Log panel in Pro mode shows it.
const entries = [];
const listeners = new Set();
const MAX = 2000;
let seq = 0;

function format(args) {
  return args.map((a) => {
    if (typeof a === 'string') return a;
    if (a instanceof Error) return a.message;
    try {
      return JSON.stringify(a);
    } catch {
      return String(a);
    }
  }).join(' ');
}

/** level: 'info' | 'warn' | 'error' | 'success'; source: 'game' | 'script' | 'ai' | 'editor' | 'system' */
export function log(level, source, ...args) {
  const entry = { id: ++seq, time: Date.now(), level, source, text: format(args).slice(0, 4000) };
  entries.push(entry);
  if (entries.length > MAX) entries.splice(0, entries.length - MAX);
  for (const fn of listeners) fn(entry);
  return entry;
}

export const logger = {
  info: (source, ...a) => log('info', source, ...a),
  warn: (source, ...a) => log('warn', source, ...a),
  error: (source, ...a) => log('error', source, ...a),
  success: (source, ...a) => log('success', source, ...a),
};

export function logEntries() {
  return entries.slice();
}

export function clearLog() {
  entries.length = 0;
  for (const fn of listeners) fn(null);
}

export function onLog(fn) {
  listeners.add(fn);
  return () => listeners.delete(fn);
}

/** Finds the line number of an error thrown inside compiled script code. */
export function scriptErrorLine(err, lineOffset = 2) {
  const stack = String(err?.stack || '');
  const m = stack.match(/<anonymous>:(\d+):(\d+)/) || stack.match(/Function:(\d+):(\d+)/);
  if (!m) return null;
  const line = parseInt(m[1], 10) - lineOffset;
  return line > 0 ? line : null;
}
