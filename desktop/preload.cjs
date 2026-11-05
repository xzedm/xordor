// Bridges the editor window and the desktop app. Only these functions are
// available to the page: it cannot touch files or keys any other way.
const { contextBridge, ipcRenderer } = require('electron');

function subscribe(channel, cb) {
  const handler = (_event, payload) => cb(payload);
  ipcRenderer.on(channel, handler);
  return () => ipcRenderer.removeListener(channel, handler);
}

contextBridge.exposeInMainWorld('xordorDesktop', {
  isDesktop: true,
  platform: process.platform,
  project: {
    open: () => ipcRenderer.invoke('project:open'),
    openPath: (file) => ipcRenderer.invoke('project:openPath', file),
    save: (file, text) => ipcRenderer.invoke('project:save', { path: file, text }),
    saveAs: (text, suggestedName) => ipcRenderer.invoke('project:saveAs', { text, suggestedName }),
    exportGame: (html, suggestedName) => ipcRenderer.invoke('game:export', { html, suggestedName }),
    recent: () => ipcRenderer.invoke('recent:list'),
    reveal: (file) => ipcRenderer.invoke('shell:reveal', file),
  },
  ai: {
    chat: (requestId, payload) => ipcRenderer.invoke('ai:chat', { requestId, ...payload }),
    cancel: (requestId) => ipcRenderer.invoke('ai:cancel', requestId),
    listModels: (settings) => ipcRenderer.invoke('ai:listModels', settings),
    onProgress: (cb) => subscribe('ai:progress', cb),
  },
  secrets: {
    status: () => ipcRenderer.invoke('secrets:status'),
    set: (provider, key, baseUrl = '') => ipcRenderer.invoke('secrets:set', { provider, key, baseUrl }),
  },
  window: {
    setTitle: (title) => ipcRenderer.send('window:title', String(title)),
    setDirty: (value) => ipcRenderer.send('window:dirty', !!value),
    setMode: (mode) => ipcRenderer.send('app:mode', mode),
    onMenu: (cb) => subscribe('menu', cb),
    onSaveAndClose: (cb) => subscribe('app:save-and-close', cb),
    closeReady: () => ipcRenderer.send('app:close-ready'),
  },
  openExternal: (url) => ipcRenderer.invoke('shell:openExternal', url),
  info: () => ipcRenderer.invoke('app:info'),
});
