// What the app can do depends on where it runs. In the Xordor desktop app the
// preload script exposes `window.xordorDesktop` (files on disk, native menus,
// secure key storage, AI requests from the desktop process). When the same
// interface is opened as a plain web page (for example a published game),
// everything falls back to browser features.
export const desktop = typeof window !== 'undefined' ? window.xordorDesktop || null : null;
export const isDesktop = !!desktop;

let requestSeq = 0;

/** Runs an AI request through the desktop app. Mirrors providers.chat(). */
export async function desktopChat(settings, { system, messages, signal, onProgress }) {
  const requestId = `ai_${Date.now()}_${++requestSeq}`;
  const off = desktop.ai.onProgress((p) => { if (p.requestId === requestId) onProgress?.(p.chars); });
  const abort = () => desktop.ai.cancel(requestId);
  signal?.addEventListener('abort', abort);
  try {
    const { keys, ...safe } = settings;
    const res = await desktop.ai.chat(requestId, { settings: safe, system, messages });
    if (res.ok) return res.text;
    if (res.error?.name === 'AbortError') throw Object.assign(new Error('aborted'), { name: 'AbortError' });
    const err = new Error(res.error?.message || 'The AI request failed.');
    err.hint = res.error?.hint || '';
    err.status = res.error?.status || 0;
    err.isAIError = true;
    throw err;
  } finally {
    off();
    signal?.removeEventListener('abort', abort);
  }
}

export async function desktopListModels(settings) {
  const { keys, ...safe } = settings;
  const res = await desktop.ai.listModels(safe);
  if (res.ok) return res.models;
  const err = new Error(res.error?.message || 'Could not load models.');
  err.hint = res.error?.hint || '';
  err.isAIError = true;
  throw err;
}
