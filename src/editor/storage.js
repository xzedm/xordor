// Saving, opening and publishing. Projects autosave in this browser, can be
// downloaded as a .xordor.json file, and can be published as one HTML file
// that anyone can open to play (no install, no internet needed).
import { normalizeProject, PROJECT_FORMAT } from '../core/project.js';
import { safeStorage } from '../core/util.js';

const AUTOSAVE_KEY = 'xordor.autosave.v2';
const storage = safeStorage();

export function autosave(project, { filePath = null, levelId = null, assetIds = [] } = {}) {
  try {
    const { assets, ...rest } = project;
    storage.setItem(AUTOSAVE_KEY, JSON.stringify({ savedAt: Date.now(), project: rest, filePath, levelId, assetIds }));
    return true;
  } catch {
    return false;
  }
}

export function loadAutosave() {
  try {
    const raw = storage.getItem(AUTOSAVE_KEY);
    if (!raw) return null;
    const data = JSON.parse(raw);
    return data?.project ? { project: normalizeProject(data.project), savedAt: data.savedAt, filePath: data.filePath || null, levelId: data.levelId || null, assetIds: Array.isArray(data.assetIds) ? data.assetIds : [] } : null;
  } catch {
    return null;
  }
}

export function fileNameFor(name, ext) {
  const base = String(name || 'my-game').toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-+|-+$/g, '') || 'my-game';
  return `${base}${ext}`;
}

export function projectToFile(project) {
  return JSON.stringify(project, null, 1);
}

/** Reads a project from a file's text. Accepts project files and published games. */
export function parseProjectFile(text) {
  const trimmed = text.trim();
  if (trimmed.startsWith('<')) {
    const m = trimmed.match(/window\.__XORDOR_GAME__\s*=\s*(\{[\s\S]*?\});?\s*<\/script>/);
    if (!m) throw new Error('This HTML file is not a Xordor game.');
    return normalizeProject(JSON.parse(m[1]));
  }
  const data = JSON.parse(trimmed);
  if (data?.format && data.format !== PROJECT_FORMAT) throw new Error('This file is not a Xordor project.');
  if (!Array.isArray(data?.levels) && !Array.isArray(data?.objects)) throw new Error('This file has no game levels in it.');
  return normalizeProject(data);
}

/** The app's own code, available in the single-file build. */
function appSource() {
  const scripts = [...document.querySelectorAll('script[type="module"]')].filter((s) => !s.src && s.textContent.length > 10000);
  const styles = [...document.querySelectorAll('style')].map((s) => s.textContent).join('\n');
  if (!scripts.length) return null;
  return { script: scripts.map((s) => s.textContent).join('\n'), styles };
}

export function canPublish() {
  return !!appSource();
}

function safeJson(value) {
  return JSON.stringify(value).replace(/</g, '\\u003c').replace(/\u2028/g, '\\u2028').replace(/\u2029/g, '\\u2029');
}

function escapeHtml(s) {
  return String(s).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]);
}

/** Builds a standalone, playable HTML file for a project. */
export function buildStandaloneHtml(project) {
  const src = appSource();
  if (!src) return null;
  const clean = normalizeProject(project);
  return `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>${escapeHtml(clean.name)}</title>
<meta name="generator" content="Xordor">
<style>${src.styles}</style>
<script>window.__XORDOR_GAME__ = ${safeJson(clean)};</script>
</head>
<body class="play-only">
<div id="app"></div>
<script type="module">${src.script}</script>
</body>
</html>
`;
}
