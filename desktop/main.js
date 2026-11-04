// Xordor desktop app (Electron main process).
// Opens the editor window, builds the native menus, reads and writes project
// files, keeps AI keys encrypted with the operating system's key store, and
// sends AI requests from here so no service can block them as "web" requests.
import { app, BrowserWindow, Menu, dialog, ipcMain, safeStorage, shell } from 'electron';
import { promises as fs, existsSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { chat, listModels, AIError, PROVIDERS } from '../src/ai/providers.js';

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, '..');
const DEV_URL = process.env.XORDOR_DEV_URL || '';
const PROJECT_EXT = 'xordor';

app.setName('Xordor');
if (process.env.XORDOR_SOFTWARE_GL) {
  // Used by automated tests on machines without a GPU.
  app.commandLine.appendSwitch('use-angle', 'swiftshader');
  app.commandLine.appendSwitch('enable-unsafe-swiftshader');
}

let win = null;
let dirty = false;
// Files the person picked in a dialog (or opened from Recent). Only these may be
// read or overwritten, so code inside a project can never touch other files.
const allowedFiles = new Set();
let allowClose = false;
let currentMode = 'simple';
const aiRequests = new Map();

const userFile = (name) => path.join(app.getPath('userData'), name);

// ---------------------------------------------------------------- small stores
async function readJson(file, fallback) {
  try {
    return JSON.parse(await fs.readFile(file, 'utf8'));
  } catch {
    return fallback;
  }
}

async function writeFileAtomic(file, text) {
  const tmp = `${file}.${process.pid}.tmp`;
  await fs.writeFile(tmp, text, 'utf8');
  await fs.rename(tmp, file);
}

async function recentList() {
  const list = await readJson(userFile('recent.json'), []);
  return (Array.isArray(list) ? list : []).filter((p) => typeof p === 'string' && existsSync(p)).slice(0, 12);
}

async function addRecent(file) {
  allowedFiles.add(file);
  const list = (await recentList()).filter((p) => p !== file);
  list.unshift(file);
  await writeFileAtomic(userFile('recent.json'), JSON.stringify(list.slice(0, 12)));
  if (process.platform === 'darwin' || process.platform === 'win32') app.addRecentDocument(file);
  buildMenu();
}

// API keys: encrypted with the system key store when available. They never leave this process.
async function readSecrets() {
  return readJson(userFile('secrets.json'), {});
}

async function getKey(provider) {
  const entry = (await readSecrets())[provider];
  if (!entry?.data) return '';
  try {
    return entry.enc ? safeStorage.decryptString(Buffer.from(entry.data, 'base64')) : Buffer.from(entry.data, 'base64').toString('utf8');
  } catch {
    return '';
  }
}

async function setKey(provider, key, baseUrl = '') {
  if (!PROVIDERS[provider]) throw new Error('Unknown AI service.');
  const secrets = await readSecrets();
  if (!key) delete secrets[provider];
  else {
    const enc = safeStorage.isEncryptionAvailable();
    secrets[provider] = { enc, data: (enc ? safeStorage.encryptString(key) : Buffer.from(key, 'utf8')).toString('base64') };
    // A key for a custom server only ever goes to the server it was saved for.
    if (PROVIDERS[provider].editableUrl) secrets[provider].baseUrl = normalizeUrl(baseUrl || PROVIDERS[provider].baseUrl);
  }
  await writeFileAtomic(userFile('secrets.json'), JSON.stringify(secrets));
}

function normalizeUrl(url) {
  return String(url || '').trim().replace(/\/+$/, '');
}

function serializeError(err) {
  if (err?.name === 'AbortError') return { name: 'AbortError', message: 'Stopped.' };
  if (err instanceof AIError) return { name: 'AIError', message: err.message, hint: err.hint, status: err.status };
  return { name: 'Error', message: err?.message || String(err) };
}

// ---------------------------------------------------------------- window
function createWindow() {
  win = new BrowserWindow({
    width: 1600,
    height: 960,
    minWidth: 1000,
    minHeight: 640,
    show: false,
    title: 'Xordor',
    backgroundColor: '#16131b',
    autoHideMenuBar: false,
    webPreferences: {
      preload: path.join(here, 'preload.cjs'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
      spellcheck: false,
    },
  });
  win.once('ready-to-show', () => win.show());

  // Links open in the normal web browser, never inside the app.
  win.webContents.setWindowOpenHandler(({ url }) => {
    if (/^https?:\/\//.test(url)) shell.openExternal(url);
    return { action: 'deny' };
  });
  win.webContents.on('will-navigate', (e, url) => {
    if (url !== win.webContents.getURL()) {
      e.preventDefault();
      if (/^https?:\/\//.test(url)) shell.openExternal(url);
    }
  });

  win.on('close', (e) => {
    if (allowClose || !dirty) return;
    e.preventDefault();
    const choice = dialog.showMessageBoxSync(win, {
      type: 'question',
      buttons: ['Save', "Don't save", 'Cancel'],
      defaultId: 0,
      cancelId: 2,
      title: 'Unsaved changes',
      message: 'Save your changes before closing?',
      detail: 'Your game has changes that are not saved to a project file yet.',
    });
    if (choice === 0) win.webContents.send('app:save-and-close');
    else if (choice === 1) {
      allowClose = true;
      win.close();
    }
  });
  win.on('closed', () => { win = null; });

  if (DEV_URL) {
    win.loadURL(DEV_URL);
  } else {
    const index = path.join(root, 'dist', 'index.html');
    if (!existsSync(index)) {
      dialog.showErrorBox('Xordor is not built yet', 'Run "npm start" in the Xordor folder. It prepares the app and then opens it.');
      app.quit();
      return;
    }
    win.loadFile(index);
  }
}

function send(command, arg) {
  win?.webContents.send('menu', { command, arg });
}

async function buildMenu() {
  const recent = await recentList();
  const isMac = process.platform === 'darwin';
  const template = [
    ...(isMac ? [{ label: 'Xordor', submenu: [{ role: 'about' }, { type: 'separator' }, { role: 'hide' }, { role: 'hideOthers' }, { type: 'separator' }, { role: 'quit' }] }] : []),
    {
      label: 'File',
      submenu: [
        { label: 'New Project…', accelerator: 'CmdOrCtrl+N', click: () => send('new') },
        { label: 'Open Project…', accelerator: 'CmdOrCtrl+O', click: () => send('open') },
        {
          label: 'Open Recent',
          enabled: recent.length > 0,
          submenu: [
            ...recent.map((p) => ({ label: path.basename(p), sublabel: p, click: () => send('open-path', p) })),
            { type: 'separator' },
            { label: 'Clear Recent', click: async () => { await writeFileAtomic(userFile('recent.json'), '[]'); buildMenu(); } },
          ],
        },
        { type: 'separator' },
        { label: 'Save', accelerator: 'CmdOrCtrl+S', click: () => send('save') },
        { label: 'Save As…', accelerator: 'CmdOrCtrl+Shift+S', click: () => send('save-as') },
        { type: 'separator' },
        { label: 'Export Playable Game…', accelerator: 'CmdOrCtrl+E', click: () => send('export') },
        { type: 'separator' },
        isMac ? { role: 'close' } : { role: 'quit', label: 'Exit' },
      ],
    },
    {
      label: 'Edit',
      submenu: [
        { label: 'Undo', accelerator: 'CmdOrCtrl+Z', click: () => send('undo') },
        { label: 'Redo', accelerator: isMac ? 'Cmd+Shift+Z' : 'Ctrl+Y', click: () => send('redo') },
        { type: 'separator' },
        { role: 'cut' }, { role: 'copy' }, { role: 'paste' },
        { type: 'separator' },
        { label: 'Duplicate', accelerator: 'CmdOrCtrl+D', click: () => send('duplicate') },
        { label: 'Delete', accelerator: 'Delete', registerAccelerator: false, click: () => send('delete') },
        { label: 'Select All', accelerator: 'CmdOrCtrl+A', click: () => send('select-all') },
        { type: 'separator' },
        { label: 'Project Settings…', click: () => send('project-settings') },
        { label: 'AI Settings…', click: () => send('ai-settings') },
      ],
    },
    {
      label: 'View',
      submenu: [
        { label: 'Simple Mode', type: 'radio', checked: currentMode === 'simple', accelerator: 'CmdOrCtrl+1', click: () => send('mode', 'simple') },
        { label: 'Pro Mode', type: 'radio', checked: currentMode === 'pro', accelerator: 'CmdOrCtrl+2', click: () => send('mode', 'pro') },
        { type: 'separator' },
        { label: 'AI Helper', accelerator: 'CmdOrCtrl+J', click: () => send('toggle-ai') },
        { label: 'Zoom to Selection', accelerator: 'F', registerAccelerator: false, click: () => send('focus') },
        { type: 'separator' },
        { role: 'togglefullscreen' },
        { role: 'resetZoom' }, { role: 'zoomIn' }, { role: 'zoomOut' },
        { type: 'separator' },
        { role: 'toggleDevTools', label: 'Developer Tools' },
      ],
    },
    {
      label: 'Play',
      submenu: [
        { label: 'Play', accelerator: 'Alt+P', click: () => send('play') },
        { label: 'Play From Camera', accelerator: 'Alt+Shift+P', click: () => send('play-here') },
        { label: 'Stop', accelerator: 'Esc', registerAccelerator: false, click: () => send('stop') },
      ],
    },
    {
      role: 'help',
      submenu: [
        { label: 'Getting Started', click: () => send('help') },
        { label: 'Keyboard Shortcuts', click: () => send('shortcuts') },
        { label: 'Script Reference', click: () => send('script-help') },
        { type: 'separator' },
        { label: `About Xordor ${app.getVersion()}`, click: () => dialog.showMessageBox(win, { title: 'About Xordor', message: `Xordor ${app.getVersion()}`, detail: 'A 3D game engine anyone can use.\nBuilt with three.js, cannon-es and Electron.' }) },
      ],
    },
  ];
  Menu.setApplicationMenu(Menu.buildFromTemplate(template));
}

// ---------------------------------------------------------------- IPC
function filters() {
  return [{ name: 'Xordor project', extensions: [PROJECT_EXT, 'json'] }];
}

function safeName(name, ext) {
  return `${String(name || 'my-game').replace(/[^\w\- ]+/g, '').trim().replace(/\s+/g, '-').toLowerCase() || 'my-game'}.${ext}`;
}

ipcMain.handle('project:open', async () => {
  const res = await dialog.showOpenDialog(win, { title: 'Open a Xordor project', properties: ['openFile'], filters: [...filters(), { name: 'Published game', extensions: ['html'] }] });
  if (res.canceled || !res.filePaths[0]) return null;
  const file = res.filePaths[0];
  allowedFiles.add(file);
  const text = await fs.readFile(file, 'utf8');
  if (!file.endsWith('.html')) await addRecent(file);
  return { path: file.endsWith('.html') ? null : file, text };
});

ipcMain.handle('project:openPath', async (_e, file) => {
  if (typeof file !== 'string' || !existsSync(file)) return null;
  if (!allowedFiles.has(file) && !(await recentList()).includes(file)) throw new Error('Open this file with File > Open Project.');
  const text = await fs.readFile(file, 'utf8');
  await addRecent(file);
  return { path: file, text };
});

ipcMain.handle('project:save', async (_e, { path: file, text }) => {
  if (typeof file !== 'string' || !file || typeof text !== 'string') throw new Error('Nothing to save.');
  if (!allowedFiles.has(file) && !(await recentList()).includes(file)) throw new Error('Choose where to save with File > Save As.');
  await writeFileAtomic(file, text);
  await addRecent(file);
  return file;
});

ipcMain.handle('project:saveAs', async (_e, { text, suggestedName }) => {
  const res = await dialog.showSaveDialog(win, {
    title: 'Save your project',
    defaultPath: path.join(app.getPath('documents'), safeName(suggestedName, PROJECT_EXT)),
    filters: filters(),
  });
  if (res.canceled || !res.filePath) return null;
  allowedFiles.add(res.filePath);
  await writeFileAtomic(res.filePath, text);
  await addRecent(res.filePath);
  return res.filePath;
});

ipcMain.handle('game:export', async (_e, { html, suggestedName }) => {
  const res = await dialog.showSaveDialog(win, {
    title: 'Export a playable game',
    defaultPath: path.join(app.getPath('documents'), safeName(suggestedName, 'html')),
    filters: [{ name: 'Playable game (HTML)', extensions: ['html'] }],
  });
  if (res.canceled || !res.filePath) return null;
  await fs.writeFile(res.filePath, html, 'utf8');
  return res.filePath;
});

ipcMain.handle('recent:list', () => recentList());
ipcMain.handle('shell:reveal', (_e, file) => { if (typeof file === 'string' && existsSync(file)) shell.showItemInFolder(file); });
ipcMain.handle('shell:openExternal', (_e, url) => { if (typeof url === 'string' && /^https?:\/\//.test(url)) shell.openExternal(url); });
ipcMain.handle('app:info', () => ({ version: app.getVersion(), platform: process.platform, secureStorage: safeStorage.isEncryptionAvailable() }));

ipcMain.handle('secrets:status', async () => {
  const s = await readSecrets();
  return { keys: Object.fromEntries(Object.keys(PROVIDERS).map((k) => [k, !!s[k]?.data])), secure: safeStorage.isEncryptionAvailable() };
});
ipcMain.handle('secrets:set', async (_e, { provider, key, baseUrl }) => {
  await setKey(provider, typeof key === 'string' ? key.trim() : '', typeof baseUrl === 'string' ? baseUrl : '');
  return true;
});

async function withKey(settings) {
  const provider = settings?.provider;
  const def = PROVIDERS[provider];
  if (!def) throw new AIError('Unknown AI service.');
  const baseUrls = { ...(settings.baseUrls || {}) };
  if (!def.editableUrl) delete baseUrls[provider]; // fixed services always use their own address
  const entry = (await readSecrets())[provider];
  let key = await getKey(provider);
  if (key && def.editableUrl && entry?.baseUrl && normalizeUrl(baseUrls[provider] || def.baseUrl) !== entry.baseUrl) {
    throw new AIError('The server address changed since the key was saved.', { hint: 'Open AI settings and enter the key again for the new address.' });
  }
  return { ...settings, baseUrls, keys: { [provider]: key } };
}

ipcMain.handle('ai:chat', async (event, { requestId, settings, system, messages }) => {
  const controller = new AbortController();
  aiRequests.set(requestId, controller);
  try {
    const text = await chat(await withKey(settings), {
      system,
      messages,
      signal: controller.signal,
      onProgress: (chars) => { if (!event.sender.isDestroyed()) event.sender.send('ai:progress', { requestId, chars }); },
    });
    return { ok: true, text };
  } catch (err) {
    return { ok: false, error: serializeError(err) };
  } finally {
    aiRequests.delete(requestId);
  }
});

ipcMain.handle('ai:cancel', (_e, requestId) => {
  aiRequests.get(requestId)?.abort();
  return true;
});

ipcMain.handle('ai:listModels', async (_e, settings) => {
  if (!PROVIDERS[settings?.provider]) return { ok: false, error: { name: 'AIError', message: 'Unknown AI service.' } };
  try {
    return { ok: true, models: await listModels(await withKey(settings)) };
  } catch (err) {
    return { ok: false, error: serializeError(err) };
  }
});

ipcMain.on('window:title', (_e, title) => { if (win && typeof title === 'string') win.setTitle(title.slice(0, 200)); });
ipcMain.on('window:dirty', (_e, value) => {
  dirty = !!value;
  win?.setDocumentEdited?.(dirty);
});
ipcMain.on('app:mode', (_e, mode) => {
  if (mode === 'simple' || mode === 'pro') {
    currentMode = mode;
    buildMenu();
  }
});
ipcMain.on('app:close-ready', () => {
  allowClose = true;
  win?.close();
});

// ---------------------------------------------------------------- start
app.whenReady().then(async () => {
  await buildMenu();
  createWindow();
  app.on('activate', () => { if (BrowserWindow.getAllWindows().length === 0) createWindow(); });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});
