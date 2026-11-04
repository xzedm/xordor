// Xordor app shell: sets up the renderer, the editor layout, play mode and
// the published "play only" mode.
import * as THREE from 'three';
import { RoomEnvironment } from 'three/addons/environments/RoomEnvironment.js';
import { WorldView } from './engine/world.js';
import './styles.css';
import { Editor } from './editor/Editor.js';
import { AddPanel, Inspector, ObjectsPanel } from './editor/panels.js';
import { AIPanel } from './editor/aiPanel.js';
import { autosave, buildStandaloneHtml, canPublish, fileNameFor, loadAutosave, parseProjectFile, projectToFile } from './editor/storage.js';
import { Game } from './engine/Game.js';
import { isMuted, setMuted } from './engine/audio.js';
import { normalizeProject, hasScripts, stripScripts } from './core/project.js';
import { buildTemplate, TEMPLATES } from './templates.js';
import { clear, confirmDialog, download, h, icon, openDialog, pickFile, toast } from './ui/dom.js';
import { debounce, safeStorage } from './core/util.js';

const storage = safeStorage();

class App {
  constructor(root) {
    this.root = root;
    this.playOnly = !!window.__XORDOR_GAME__;
    this.game = null;
    this.timer = new THREE.Timer();

    this.viewport = h('div', { class: 'viewport' });
    this.canvas = h('canvas', { class: 'view-canvas', tabIndex: 0, 'aria-label': '3D view' });
    this.viewport.append(this.canvas);
    try {
      this.renderer = new THREE.WebGLRenderer({ canvas: this.canvas, antialias: true, powerPreference: 'high-performance' });
    } catch (err) {
      this.showFatal('Your browser could not start 3D graphics (WebGL). Try another browser, or turn on hardware acceleration in your browser settings.');
      throw err;
    }
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
    this.renderer.shadowMap.enabled = true;
    this.renderer.shadowMap.type = THREE.PCFShadowMap;
    this.renderer.toneMapping = THREE.ACESFilmicToneMapping;
    this.renderer.toneMappingExposure = 1;
    try {
      const pmrem = new THREE.PMREMGenerator(this.renderer);
      WorldView.environmentMap = pmrem.fromScene(new RoomEnvironment(), 0.04).texture;
      pmrem.dispose();
    } catch { /* reflections are optional */ }

    if (this.playOnly) this.startPlayOnly();
    else this.startEditor();

    new ResizeObserver(() => this.resize()).observe(this.viewport);
    this.resize();
    this.renderer.setAnimationLoop(() => this.frame());
  }

  showFatal(text) {
    clear(this.root);
    this.root.append(h('div', { class: 'fatal' }, h('h1', {}, 'Xordor'), h('p', {}, text)));
  }

  get active() {
    return this.game || this.editor;
  }

  frame() {
    this.timer.update();
    const dt = this.timer.getDelta();
    const a = this.active;
    if (!a) return;
    a.frame(dt);
    a.render(this.renderer);
  }

  resize() {
    const r = this.viewport.getBoundingClientRect();
    const w = Math.max(1, Math.floor(r.width));
    const hgt = Math.max(1, Math.floor(r.height));
    this.renderer.setSize(w, hgt, false);
    this.editor?.resize(w, hgt);
    this.game?.resize(w, hgt);
  }

  // ------------------------------------------------------------ published game
  startPlayOnly() {
    document.body.classList.add('play-only');
    this.root.append(this.viewport);
    const project = normalizeProject(window.__XORDOR_GAME__);
    document.title = project.name;
    const start = () => {
      this.game?.dispose();
      this.game = new Game({ container: this.viewport, canvas: this.canvas, project, playOnly: true });
      this.game.onRestart = start;
      this.resize();
    };
    start();
  }

  // ------------------------------------------------------------ editor
  startEditor() {
    const root = this.root;
    root.classList.add('studio');
    root.dataset.tab = 'add';

    this.titleInput = h('input', { class: 'project-title', type: 'text', 'aria-label': 'Game name', value: 'My Game' });
    this.saveState = h('span', { class: 'save-state muted small', 'aria-live': 'polite' });
    this.undoBtn = h('button', { type: 'button', class: 'icon-btn', title: 'Undo (Ctrl+Z)', 'aria-label': 'Undo', onclick: () => this.editor.undo() }, icon('undo'));
    this.redoBtn = h('button', { type: 'button', class: 'icon-btn', title: 'Redo (Ctrl+Shift+Z)', 'aria-label': 'Redo', onclick: () => this.editor.redo() }, icon('redo'));
    this.playBtn = h('button', { type: 'button', class: 'btn btn-play', title: 'Play your game (P)', onclick: () => (this.game ? this.stopPlaying() : this.play()) });
    this.updatePlayButton();
    this.muteBtn = h('button', { type: 'button', class: 'icon-btn mute-btn', title: 'Sound on/off', 'aria-label': 'Sound on or off', onclick: () => this.toggleMute() }, icon('sound'));
    this.aiToggle = h('button', { type: 'button', class: 'btn btn-ai', 'aria-pressed': 'true', onclick: () => this.toggleAI() }, icon('sparkle'), h('span', {}, 'AI helper'));

    const menuBtn = (label, ic, fn, title) => h('button', { type: 'button', class: 'btn btn-ghost', title: title || label, onclick: fn }, icon(ic), h('span', { class: 'label' }, label));
    const topbar = h('header', { class: 'topbar' },
      h('div', { class: 'brand' }, h('span', { class: 'brand-mark', html: LOGO }), h('span', { class: 'brand-name' }, 'Xordor')),
      h('div', { class: 'title-wrap' }, this.titleInput, this.saveState),
      h('nav', { class: 'menu', 'aria-label': 'File' },
        menuBtn('New', 'file', () => this.newProject(), 'Start a new game'),
        menuBtn('Open', 'open', () => this.openFile(), 'Open a saved game file'),
        menuBtn('Save', 'save', () => this.saveFile(), 'Save your game as a file (Ctrl+S)'),
        menuBtn('Publish', 'publish', () => this.publish(), 'Make a playable file to share')),
      h('div', { class: 'top-actions' }, this.undoBtn, this.redoBtn, this.muteBtn,
        h('button', { type: 'button', class: 'icon-btn', title: 'Help', 'aria-label': 'Help', onclick: () => this.help() }, icon('help')),
        this.aiToggle, this.playBtn));
    this.titleInput.addEventListener('change', () => this.editor.rename(this.titleInput.value));

    // Panels
    const leftTabs = h('div', { class: 'tabs', role: 'tablist' });
    const addRoot = h('div', { class: 'panel-body', id: 'panel-add', role: 'tabpanel' });
    const objectsRoot = h('div', { class: 'panel-body', id: 'panel-objects', role: 'tabpanel', hidden: true });
    const setLeft = (which) => {
      addRoot.hidden = which !== 'add';
      objectsRoot.hidden = which !== 'objects';
      for (const b of leftTabs.children) b.setAttribute('aria-selected', b.dataset.tab === which ? 'true' : 'false');
    };
    leftTabs.append(
      h('button', { type: 'button', role: 'tab', class: 'tab', dataset: { tab: 'add' }, 'aria-selected': 'true', onclick: () => setLeft('add') }, icon('plus'), 'Add'),
      h('button', { type: 'button', role: 'tab', class: 'tab', dataset: { tab: 'objects' }, 'aria-selected': 'false', onclick: () => setLeft('objects') }, icon('list'), 'Objects'));
    const left = h('aside', { class: 'side side-left', 'aria-label': 'Add and objects' }, leftTabs, addRoot, objectsRoot);

    const inspectorRoot = h('div', { class: 'panel-body inspector' });
    const right = h('aside', { class: 'side side-right', 'aria-label': 'Properties' }, h('div', { class: 'tabs' }, h('span', { class: 'tab static' }, icon('sliders'), 'Properties')), inspectorRoot);

    const aiRoot = h('aside', { class: 'side side-ai', 'aria-label': 'AI helper' });

    // Viewport tools
    this.toolButtons = {};
    const tool = (mode, ic, label, key) => {
      const b = h('button', { type: 'button', class: 'tool', title: `${label} (${key})`, 'aria-label': label, onclick: () => this.editor.setGizmoMode(mode) }, icon(ic), h('span', {}, label));
      this.toolButtons[mode] = b;
      return b;
    };
    this.snapBtn = h('button', { type: 'button', class: 'tool', title: 'Snap to grid', 'aria-pressed': 'true', onclick: () => this.editor.setSnap(!this.editor.snap) }, icon('magnet'), h('span', {}, 'Snap'));
    const toolbar = h('div', { class: 'view-tools', role: 'toolbar', 'aria-label': 'Edit tools' },
      tool('translate', 'move', 'Move', 'W'), tool('rotate', 'rotate', 'Turn', 'E'), tool('scale', 'scale', 'Size', 'R'),
      h('span', { class: 'tool-sep' }), this.snapBtn,
      h('button', { type: 'button', class: 'tool', title: 'Duplicate (Ctrl+D)', 'aria-label': 'Duplicate', onclick: () => this.editor.duplicateSelected() }, icon('copy')),
      h('button', { type: 'button', class: 'tool', title: 'Delete (Del)', 'aria-label': 'Delete', onclick: () => this.editor.deleteSelected() }, icon('trash')),
      h('button', { type: 'button', class: 'tool', title: 'Zoom to selection (F)', 'aria-label': 'Zoom to selection', onclick: () => this.editor.focusSelected() }, icon('focus')));
    this.hint = h('div', { class: 'view-hint' }, 'Drag to look around. Right-drag to slide. Scroll to zoom. Click something to select it.');
    this.emptyState = h('div', { class: 'empty-state', hidden: true },
      h('strong', {}, 'Your world is empty'),
      h('p', {}, 'Pick something from the Add panel, or ask the AI helper to build for you.'));
    this.viewport.append(toolbar, this.hint, this.emptyState);

    // Phone layout: a tab bar switches between panels.
    const mobileTab = (tab, ic, label) => h('button', { type: 'button', class: 'mtab', dataset: { tab }, onclick: () => this.setMobileTab(tab) }, icon(ic), h('span', {}, label));
    const mobileBar = h('nav', { class: 'mobile-tabs', 'aria-label': 'Panels' },
      mobileTab('add', 'plus', 'Add'), mobileTab('objects', 'list', 'Objects'), mobileTab('props', 'sliders', 'Edit'), mobileTab('ai', 'sparkle', 'AI'));
    this.setLeft = setLeft;

    root.append(topbar, left, this.viewport, right, aiRoot, mobileBar);

    // Editor + panels
    this.editor = new Editor({ canvas: this.canvas, container: this.viewport });
    this.addPanel = new AddPanel(addRoot, this.editor);
    this.objectsPanel = new ObjectsPanel(objectsRoot, this.editor);
    this.ai = new AIPanel(aiRoot, this.editor);
    this.inspector = new Inspector(inspectorRoot, this.editor, {
      onAskAI: (e) => { this.showAI(true); this.setMobileTab('ai'); this.ai.focus(`Change "${e.name}": `); },
    });

    const save = debounce((p) => {
      const ok = autosave(p);
      this.saveState.textContent = ok ? 'Saved in this browser' : 'Not saved (browser storage is off)';
    }, 600);
    this.editor.on('change', ({ source }) => {
      const p = this.editor.project;
      if (document.activeElement !== this.titleInput) this.titleInput.value = p.name;
      this.undoBtn.disabled = !this.editor.undoStack.length;
      this.redoBtn.disabled = !this.editor.redoStack.length;
      this.emptyState.hidden = p.objects.some((o) => o.shape !== 'spawn');
      if (source !== 'load') this.saveState.textContent = 'Saving...';
      save(p);
    });
    this.editor.on('select', (id) => {
      if (id && root.dataset.tab === 'add' && window.matchMedia('(max-width: 860px)').matches) this.setMobileTab('props');
    });
    this.editor.on('tool', (mode) => this.updateTools(mode));
    this.updateTools(this.editor.gizmoMode);
    this.bindKeys();

    try { if (storage.getItem('xordor.muted') === '1') { setMuted(true); this.updateMute(); } } catch { /* optional */ }
    try { if (storage.getItem('xordor.aiHidden') === '1') this.showAI(false); } catch { /* optional */ }

    const saved = loadAutosave();
    if (saved && saved.project.objects.length) {
      this.editor.load(saved.project);
      this.editor.focusSelected();
      this.saveState.textContent = 'Saved in this browser';
    } else {
      this.editor.load(buildTemplate('coins'));
      this.editor.focusSelected();
      this.welcome();
    }
    this.setMobileTab('add');
  }

  updatePlayButton() {
    clear(this.playBtn);
    const playing = !!this.game;
    this.playBtn.append(icon(playing ? 'stop' : 'play'), h('span', {}, playing ? 'Stop' : 'Play'));
    this.playBtn.classList.toggle('is-playing', playing);
  }

  setMobileTab(tab) {
    this.root.dataset.tab = tab;
    if (tab === 'add' || tab === 'objects') this.setLeft?.(tab);
    for (const b of this.root.querySelectorAll('.mtab')) b.setAttribute('aria-current', b.dataset.tab === tab ? 'page' : 'false');
    requestAnimationFrame(() => this.resize());
  }

  showAI(on) {
    this.root.classList.toggle('ai-hidden', !on);
    this.aiToggle.setAttribute('aria-pressed', on ? 'true' : 'false');
    try { storage.setItem('xordor.aiHidden', on ? '0' : '1'); } catch { /* optional */ }
    requestAnimationFrame(() => this.resize());
  }

  toggleAI() {
    const hidden = this.root.classList.contains('ai-hidden');
    this.showAI(hidden);
    if (hidden) this.ai.focus();
  }

  toggleMute() {
    setMuted(!isMuted());
    try { storage.setItem('xordor.muted', isMuted() ? '1' : '0'); } catch { /* optional */ }
    this.updateMute();
  }

  updateMute() {
    clear(this.muteBtn);
    this.muteBtn.append(icon(isMuted() ? 'mute' : 'sound'));
    this.muteBtn.setAttribute('aria-label', isMuted() ? 'Sound is off' : 'Sound is on');
  }

  updateTools(mode) {
    for (const [m, b] of Object.entries(this.toolButtons)) b.setAttribute('aria-pressed', m === mode ? 'true' : 'false');
    this.snapBtn.setAttribute('aria-pressed', this.editor.snap ? 'true' : 'false');
  }

  bindKeys() {
    window.addEventListener('keydown', (e) => {
      if (this.game) return;
      if (document.querySelector('.dialog-backdrop')) return;
      const typing = e.target instanceof HTMLElement && e.target.closest('input, textarea, select, [contenteditable]');
      const mod = e.ctrlKey || e.metaKey;
      const ed = this.editor;
      if (mod && e.key.toLowerCase() === 's') { e.preventDefault(); this.saveFile(); return; }
      if (typing) return;
      if (mod && e.key.toLowerCase() === 'z') { e.preventDefault(); if (e.shiftKey) ed.redo(); else ed.undo(); return; }
      if (mod && e.key.toLowerCase() === 'y') { e.preventDefault(); ed.redo(); return; }
      if (mod && e.key.toLowerCase() === 'd') { e.preventDefault(); ed.duplicateSelected(); return; }
      if (mod) return;
      switch (e.key) {
        case 'Delete':
        case 'Backspace': if (ed.selectedId) { e.preventDefault(); ed.deleteSelected(); } break;
        case 'w': case 'W': ed.setGizmoMode('translate'); break;
        case 'e': case 'E': ed.setGizmoMode('rotate'); break;
        case 'r': case 'R': ed.setGizmoMode('scale'); break;
        case 'f': case 'F': ed.focusSelected(); break;
        case 'p': case 'P': this.play(); break;
        case 'Escape': ed.select(null); break;
        default:
      }
    });
  }

  // ------------------------------------------------------------ play mode
  play() {
    if (this.game) return;
    const project = this.editor.project;
    this.root.classList.add('playing');
    this.editor.setActive(false);
    this.editor.select(null);
    const start = () => {
      this.game?.dispose();
      this.game = new Game({ container: this.viewport, canvas: this.canvas, project, onExit: () => this.stopPlaying() });
      this.game.onRestart = start;
      this.resize();
    };
    start();
    this.updatePlayButton();
    this.setMobileTab('play');
  }

  stopPlaying() {
    if (!this.game) return;
    this.game.dispose();
    this.game = null;
    this.root.classList.remove('playing');
    this.updatePlayButton();
    this.editor.setActive(true);
    this.setMobileTab('add');
    this.resize();
    this.canvas.focus();
  }

  // ------------------------------------------------------------ files
  templatePicker({ title, onPick, intro }) {
    const grid = h('div', { class: 'template-grid' }, TEMPLATES.map((t) => h('button', {
      type: 'button', class: 'template-card', onclick: () => { dialog.close(); onPick(t.key); },
    }, h('span', { class: `template-art art-${t.key}` }), h('strong', {}, t.label), h('span', { class: 'muted small' }, t.blurb))));
    const dialog = openDialog({ title, body: h('div', {}, intro, grid), wide: true });
    return dialog;
  }

  welcome() {
    if (document.querySelector('.dialog-backdrop')) return;
    const intro = h('div', { class: 'welcome' },
      h('p', { class: 'lead' }, 'Make 3D games without writing code. Pick a starting point, change anything you like, then press Play.'),
      h('ol', { class: 'steps' },
        h('li', {}, h('strong', {}, 'Add'), ' things from the left panel, or ask the ', h('strong', {}, 'AI helper'), ' to build them.'),
        h('li', {}, h('strong', {}, 'Click'), ' anything to move it, recolor it or give it behaviors like "spin" or "collect for points".'),
        h('li', {}, 'Press ', h('strong', {}, 'Play'), ' to try your game, then ', h('strong', {}, 'Publish'), ' to share it as one file.')));
    this.templatePicker({ title: 'Welcome to Xordor', intro, onPick: (key) => this.editor.load(buildTemplate(key)) || this.editor.focusSelected() });
  }

  async newProject() {
    this.templatePicker({
      title: 'Start a new game',
      intro: h('p', { class: 'muted' }, 'Your current game stays saved until you change the new one. Save it as a file first if you want to keep it.'),
      onPick: (key) => {
        this.editor.load(buildTemplate(key));
        this.editor.focusSelected();
        toast(`Started "${TEMPLATES.find((t) => t.key === key).label}"`);
      },
    });
  }

  async openFile() {
    const file = await pickFile('.json,.xordor,.html,application/json,text/html');
    if (!file) return;
    let project;
    try {
      project = parseProjectFile(await file.text());
    } catch (err) {
      toast(`Couldn't open that file. ${err.message}`, { kind: 'error', ms: 5000 });
      return;
    }
    if (hasScripts(project)) {
      const keep = await confirmDialog('This game has custom code',
        'This file contains custom code that runs while playing. Only keep it if you trust the person who made the file. Custom code could misuse things saved in this browser, such as your AI keys.',
        { ok: 'Keep the code', cancel: 'Remove the code', danger: true });
      if (!keep) stripScripts(project);
    }
    this.editor.load(project);
    this.editor.focusSelected();
    toast(`Opened "${project.name}"`);
  }

  saveFile() {
    const p = this.editor.project;
    download(fileNameFor(p.name, '.xordor.json'), projectToFile(p), 'application/json');
    toast(`Downloading ${fileNameFor(p.name, '.xordor.json')}`);
  }

  publish() {
    const p = this.editor.project;
    if (!canPublish()) {
      openDialog({
        title: 'Publish your game',
        body: h('div', {},
          h('p', {}, 'Publishing makes one HTML file that anyone can open to play your game.'),
          h('p', { class: 'muted' }, 'This copy of Xordor is running in developer mode, which cannot bundle itself. Run "npm run build" and open dist/index.html, or use the hosted version, to publish.')),
        actions: [{ label: 'OK', primary: true }],
      });
      return;
    }
    const name = fileNameFor(p.name, '.html');
    openDialog({
      title: 'Publish your game',
      body: h('div', { class: 'publish' },
        h('p', {}, `Xordor will make one file, `, h('strong', {}, name), `, with your whole game inside.`),
        h('ul', { class: 'steps' },
          h('li', {}, 'Anyone can double-click it to play in a web browser. No install, no internet needed.'),
          h('li', {}, 'Share it by email or chat, or upload it to a free host such as GitHub Pages, Netlify Drop or itch.io (as an HTML game).'),
          h('li', {}, 'Your AI keys are never included.'))),
      actions: [
        { label: 'Cancel', value: null },
        {
          label: 'Download game', primary: true, run: () => {
            const html = buildStandaloneHtml(p);
            if (!html) { toast('Publishing is not available here.', { kind: 'error' }); return false; }
            download(name, html, 'text/html');
            toast(`Downloading ${name}`);
            return true;
          },
        },
      ],
    });
  }

  help() {
    openDialog({
      title: 'How to use Xordor',
      wide: true,
      body: h('div', { class: 'help' },
        h('div', { class: 'help-cols' },
          h('section', {},
            h('h3', {}, 'Building'),
            h('ul', {},
              h('li', {}, 'Click a thing in the ', h('strong', {}, 'Add'), ' panel to drop it in front of you.'),
              h('li', {}, 'Click an object to select it. Drag the colored arrows to move it.'),
              h('li', {}, h('kbd', {}, 'W'), ' move, ', h('kbd', {}, 'E'), ' turn, ', h('kbd', {}, 'R'), ' size, ', h('kbd', {}, 'F'), ' zoom to it'),
              h('li', {}, h('kbd', {}, 'Ctrl'), '+', h('kbd', {}, 'D'), ' duplicate, ', h('kbd', {}, 'Del'), ' delete, ', h('kbd', {}, 'Ctrl'), '+', h('kbd', {}, 'Z'), ' undo'),
              h('li', {}, 'Drag empty space to look around, right-drag to slide, scroll to zoom.'),
              h('li', {}, 'Click empty space to see the world settings: sky, ground and game rules.'))),
          h('section', {},
            h('h3', {}, 'Playing'),
            h('ul', {},
              h('li', {}, h('kbd', {}, 'W'), h('kbd', {}, 'A'), h('kbd', {}, 'S'), h('kbd', {}, 'D'), ' or arrow keys to move, ', h('kbd', {}, 'Space'), ' to jump'),
              h('li', {}, 'Hold ', h('kbd', {}, 'Shift'), ' to run. Drag the mouse to look. ', h('kbd', {}, 'Q'), '/', h('kbd', {}, 'E'), ' turn the camera.'),
              h('li', {}, h('kbd', {}, 'Esc'), ' goes back to the editor.'),
              h('li', {}, 'On phones and tablets, use the on-screen stick and Jump button.'))),
          h('section', {},
            h('h3', {}, 'AI helper'),
            h('ul', {},
              h('li', {}, 'Type what you want, like "add a bridge made of crates" or "make enemies faster".'),
              h('li', {}, 'Select an object first to change just that object.'),
              h('li', {}, 'The built-in helper is free and works offline. For full creative help, open AI settings and connect ChatGPT, Claude, Gemini, OpenRouter, or a free local model with Ollama.'),
              h('li', {}, 'Every AI change can be undone with one click.'))))),
      actions: [{ label: 'Got it', primary: true }],
    });
  }
}

const LOGO = '<svg viewBox="0 0 32 32" aria-hidden="true"><path d="M16 2 29 9.5v13L16 30 3 22.5v-13z" fill="var(--accent)"/><path d="M16 2 29 9.5 16 17 3 9.5z" fill="var(--accent-2)"/><path d="m10 12 12 8M22 12l-12 8" stroke="var(--ink-on-accent)" stroke-width="2.6" stroke-linecap="round"/></svg>';

const root = document.getElementById('app');
try {
  window.xordor = new App(root);
} catch (err) {
  console.error(err);
}
