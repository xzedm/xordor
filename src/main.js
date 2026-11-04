// Xordor app shell: the renderer, the two editor modes (Simple and Pro), play
// mode, project files, desktop menus, and the "play only" mode of published games.
import * as THREE from 'three';
import { RoomEnvironment } from 'three/addons/environments/RoomEnvironment.js';
import '@fontsource/fredoka/latin-500.css';
import '@fontsource/fredoka/latin-600.css';
import '@fontsource/atkinson-hyperlegible/latin-400.css';
import '@fontsource/atkinson-hyperlegible/latin-700.css';
import '@fontsource/jetbrains-mono/latin-400.css';
import './styles.css';
import { WorldView } from './engine/world.js';
import { Editor } from './editor/Editor.js';
import { AddPanel, Inspector, ObjectsPanel } from './editor/panels.js';
import { AIPanel } from './editor/aiPanel.js';
import { CodePanel, ContentBrowser, OutlinerPanel, OutputLog, SCRIPT_REFERENCE, StatsOverlay, ViewportBar, askText } from './editor/proPanels.js';
import { checkField, numberField, rangeField, selectField, textField, colorField } from './editor/fields.js';
import { autosave, buildStandaloneHtml, canPublish, fileNameFor, loadAutosave, parseProjectFile, projectToFile } from './editor/storage.js';
import { Game } from './engine/Game.js';
import { isMuted, setMuted } from './engine/audio.js';
import { CAMERAS, WIN_CONDITIONS, hasScripts, normalizeProject, stripScripts } from './core/project.js';
import { buildTemplate, TEMPLATES } from './templates.js';
import { clear, confirmDialog, download, h, icon, openDialog, pickFile, toast } from './ui/dom.js';
import { debounce, safeStorage } from './core/util.js';
import { logger } from './core/log.js';
import { desktop, isDesktop } from './platform.js';

const storage = safeStorage();
const pref = {
  get: (k, d = null) => { try { return storage.getItem(`xordor.${k}`) ?? d; } catch { return d; } },
  set: (k, v) => { try { storage.setItem(`xordor.${k}`, v); } catch { /* optional */ } },
};
const isTyping = (el) => el instanceof HTMLElement && !!el.closest('input, textarea, select, [contenteditable], .cm-editor');
const narrow = () => window.matchMedia('(max-width: 860px)').matches;

class App {
  constructor(root) {
    this.root = root;
    this.playOnly = !!window.__XORDOR_GAME__;
    this.game = null;
    this.timer = new THREE.Timer();
    this.filePath = null;

    this.viewport = h('div', { class: 'viewport' });
    this.canvas = h('canvas', { class: 'view-canvas', tabIndex: 0, 'aria-label': '3D view' });
    this.viewport.append(this.canvas);
    try {
      this.renderer = new THREE.WebGLRenderer({ canvas: this.canvas, antialias: true, powerPreference: 'high-performance' });
    } catch (err) {
      this.showFatal('Xordor could not start 3D graphics (WebGL). Update your graphics drivers, or turn on hardware acceleration.');
      throw err;
    }
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
    this.renderer.shadowMap.enabled = true;
    this.renderer.shadowMap.type = THREE.PCFShadowMap;
    this.renderer.toneMapping = THREE.ACESFilmicToneMapping;
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
    this.stats?.update(dt, this.renderer, this.game ? this.game.runtime.length : this.editor.project?.objects.length || 0);
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
    const start = (levelId = project.startLevel, carry = null) => {
      this.game?.dispose();
      this.game = new Game({ container: this.viewport, canvas: this.canvas, project, levelId, carry, playOnly: true });
      this.game.onRestart = () => start();
      this.game.onNextLevel = (next, c) => { start(next, c); this.game.begin(); };
      this.resize();
    };
    start();
  }

  // ------------------------------------------------------------ editor
  startEditor() {
    document.body.classList.toggle('desktop', isDesktop);
    this.editor = new Editor({ canvas: this.canvas, container: this.viewport });
    const ed = this.editor;

    // Shared panels (they move between layouts).
    this.inspectorRoot = h('div', { class: 'panel-body inspector' });
    this.aiRoot = h('div', { class: 'ai-host' });
    this.inspector = new Inspector(this.inspectorRoot, ed, {
      onAskAI: (e) => this.openAI(`Change "${e.name}": `),
      onOpenCode: (target) => this.openCode(target),
      onSavePrefab: () => this.savePrefab(),
    });
    this.ai = new AIPanel(this.aiRoot, ed);

    this.buildViewportOverlays();
    this.buildSimpleLayout();
    this.buildProLayout();

    const save = debounce(() => {
      autosave(ed.doc, { filePath: this.filePath, levelId: ed.levelId });
      if (!isDesktop) this.setSaveState('Saved in this app');
    }, 700);
    ed.on('change', ({ source }) => {
      this.updateChrome();
      if (source !== 'load') save();
    });
    ed.on('level', () => this.updateChrome());
    ed.on('saved', () => this.updateChrome());
    ed.on('tool', () => this.updateTools());
    ed.on('select', (id) => {
      if (id && this.mode === 'simple' && this.root.dataset.tab === 'add' && narrow()) this.setMobileTab('props');
    });
    this.bindKeys();
    this.bindDesktop();

    if (pref.get('muted') === '1') { setMuted(true); }
    if (pref.get('aiHidden') === '1') this.root.classList.add('ai-hidden');

    const saved = loadAutosave();
    if (saved && saved.project.levels.some((l) => l.objects.length)) {
      this.filePath = saved.filePath || null;
      ed.load(saved.project, { levelId: saved.levelId });
      if (isDesktop && saved.filePath) ed.markSaved();
    } else {
      ed.load(buildTemplate('coins'));
    }
    ed.focusSelected();

    const mode = pref.get('mode');
    this.applyMode(mode === 'pro' && !narrow() ? 'pro' : 'simple');
    logger.info('system', `Xordor ${isDesktop ? 'desktop app' : 'web'} started in ${this.mode === 'pro' ? 'Pro' : 'Simple'} mode.`);
    if (!mode) this.chooseMode(true);
    else if (!saved) this.startScreen();
    window.addEventListener('resize', () => { if (this.mode === 'pro' && narrow()) this.applyMode('simple'); });
  }

  setSaveState(text) {
    if (this.saveStateEls) for (const el of this.saveStateEls) el.textContent = text;
  }

  // ---------- shared UI bits ----------
  modeToggle() {
    const wrap = h('div', { class: 'mode-toggle', role: 'radiogroup', 'aria-label': 'Editor mode' },
      ['simple', 'pro'].map((m) => h('button', {
        type: 'button', role: 'radio', class: 'mode-btn', dataset: { mode: m },
        title: m === 'simple' ? 'Simple mode: friendly tools for everyone' : 'Pro mode: the full editor for developers',
        onclick: () => this.applyMode(m),
      }, m === 'simple' ? 'Simple' : 'Pro')));
    (this.modeToggles ||= []).push(wrap);
    return wrap;
  }

  levelSwitcher() {
    const select = h('select', { class: 'level-select', 'aria-label': 'Level' });
    select.addEventListener('change', () => {
      if (select.value === '__new') { select.value = this.editor.levelId; this.addLevel(); } else this.editor.switchLevel(select.value);
    });
    (this.levelSelects ||= []).push(select);
    return h('label', { class: 'level-wrap', title: 'Which level you are editing' }, icon('globe'), select);
  }

  updateLevelSelects() {
    const doc = this.editor.doc;
    for (const s of this.levelSelects || []) {
      clear(s);
      doc.levels.forEach((l, i) => s.append(h('option', { value: l.id, selected: l.id === this.editor.levelId }, `${i + 1}. ${l.name}`)));
      s.append(h('option', { value: '__new' }, '+ Add a level'));
      s.value = this.editor.levelId;
    }
  }

  async addLevel() {
    const name = await askText('Add a level', { label: 'Level name', value: `Level ${this.editor.doc.levels.length + 1}`, ok: 'Add level' });
    if (name === null) return;
    const level = this.editor.addLevel(name || null);
    toast(`Added "${level.name}". Winning a level moves on to the next one.`);
  }

  playControls() {
    const play = h('button', { type: 'button', class: 'btn btn-play', title: 'Play (Alt+P)', onclick: () => (this.game ? this.stopPlaying() : this.play()) });
    (this.playButtons ||= []).push(play);
    return play;
  }

  updatePlayButtons() {
    const playing = !!this.game;
    for (const b of this.playButtons || []) {
      clear(b);
      b.append(icon(playing ? 'stop' : 'play'), h('span', {}, playing ? 'Stop' : 'Play'));
      b.classList.toggle('is-playing', playing);
    }
    if (this.pauseBtn) {
      this.pauseBtn.disabled = !playing;
      clear(this.pauseBtn);
      this.pauseBtn.append(icon(this.game?.paused ? 'play' : 'pause'));
      this.pauseBtn.setAttribute('aria-label', this.game?.paused ? 'Resume' : 'Pause');
    }
    if (this.playHereBtn) this.playHereBtn.disabled = playing;
  }

  updateChrome() {
    const ed = this.editor;
    const p = ed.project;
    if (!p) return;
    for (const t of this.titleInputs || []) if (document.activeElement !== t) t.value = p.name;
    for (const b of this.undoButtons || []) b.disabled = !ed.undoStack.length;
    for (const b of this.redoButtons || []) b.disabled = !ed.redoStack.length;
    this.emptyState.hidden = p.objects.some((o) => o.shape !== 'spawn');
    this.updateLevelSelects();
    const file = this.filePath ? this.filePath.split(/[\\/]/).pop() : '';
    if (isDesktop) {
      this.setSaveState(ed.dirty ? (file ? 'Unsaved changes' : 'Not saved to a file yet') : file ? `Saved to ${file}` : '');
      desktop.window.setDirty(ed.dirty);
      desktop.window.setTitle(`${ed.dirty ? '• ' : ''}${p.name}${file ? ` (${file})` : ''} - Xordor`);
    } else {
      document.title = `${p.name} - Xordor`;
    }
  }

  updateTools() {
    const ed = this.editor;
    for (const [m, b] of Object.entries(this.toolButtons || {})) b.setAttribute('aria-pressed', m === ed.gizmoMode ? 'true' : 'false');
    this.snapBtn?.setAttribute('aria-pressed', ed.snap ? 'true' : 'false');
  }

  // ---------- viewport overlays ----------
  buildViewportOverlays() {
    const ed = this.editor;
    this.toolButtons = {};
    const tool = (mode, ic, label, key) => {
      const b = h('button', { type: 'button', class: 'tool', title: `${label} (${key})`, 'aria-label': label, onclick: () => ed.setGizmoMode(mode) }, icon(ic), h('span', {}, label));
      this.toolButtons[mode] = b;
      return b;
    };
    this.snapBtn = h('button', { type: 'button', class: 'tool', title: 'Snap to grid', 'aria-pressed': 'true', onclick: () => ed.setSnap(!ed.snap) }, icon('magnet'), h('span', {}, 'Snap'));
    const simpleTools = h('div', { class: 'view-tools simple-only', role: 'toolbar', 'aria-label': 'Edit tools' },
      tool('translate', 'move', 'Move', 'W'), tool('rotate', 'rotate', 'Turn', 'E'), tool('scale', 'scale', 'Size', 'R'),
      h('span', { class: 'tool-sep' }), this.snapBtn,
      h('button', { type: 'button', class: 'tool', title: 'Duplicate (Ctrl+D)', 'aria-label': 'Duplicate', onclick: () => ed.duplicateSelected() }, icon('copy')),
      h('button', { type: 'button', class: 'tool', title: 'Delete (Del)', 'aria-label': 'Delete', onclick: () => ed.deleteSelected() }, icon('trash')),
      h('button', { type: 'button', class: 'tool', title: 'Zoom to selection (F)', 'aria-label': 'Zoom to selection', onclick: () => ed.focusSelected() }, icon('focus')));
    this.hint = h('div', { class: 'view-hint simple-only' }, 'Drag to look around. Right-drag to slide. Scroll to zoom. Click something to select it.');
    this.proHint = h('div', { class: 'view-hint pro-only' }, 'Hold right mouse + W A S D Q E to fly · left-drag orbits · middle-drag pans · Ctrl+click to multi-select');
    this.emptyState = h('div', { class: 'empty-state', hidden: true },
      h('strong', {}, 'This level is empty'),
      h('p', {}, 'Add something from the panel on the left, or ask the AI helper to build for you.'));
    const vbar = h('div', { class: 'viewport-bar pro-only', role: 'toolbar', 'aria-label': 'Viewport' });
    const statsEl = h('div', { class: 'stats-overlay pro-only' });
    this.stats = new StatsOverlay(statsEl);
    this.viewportBar = new ViewportBar(vbar, ed, { onToggleStats: (on) => { statsEl.hidden = !on; } });
    this.viewport.append(simpleTools, this.hint, this.proHint, this.emptyState, vbar, statsEl);
  }

  topButton(label, ic, fn, title) {
    return h('button', { type: 'button', class: 'btn btn-ghost', title: title || label, onclick: fn }, icon(ic), h('span', { class: 'label' }, label));
  }

  undoRedo() {
    const u = h('button', { type: 'button', class: 'icon-btn', title: 'Undo (Ctrl+Z)', 'aria-label': 'Undo', onclick: () => this.editor.undo() }, icon('undo'));
    const r = h('button', { type: 'button', class: 'icon-btn', title: 'Redo (Ctrl+Y)', 'aria-label': 'Redo', onclick: () => this.editor.redo() }, icon('redo'));
    (this.undoButtons ||= []).push(u);
    (this.redoButtons ||= []).push(r);
    return [u, r];
  }

  titleInput() {
    const input = h('input', { class: 'project-title', type: 'text', 'aria-label': 'Game name', value: 'My Game' });
    input.addEventListener('change', () => this.editor.rename(input.value));
    (this.titleInputs ||= []).push(input);
    return input;
  }

  saveStateEl() {
    const el = h('span', { class: 'save-state muted small', 'aria-live': 'polite' });
    (this.saveStateEls ||= []).push(el);
    return el;
  }

  // ---------- Simple layout ----------
  buildSimpleLayout() {
    const ed = this.editor;
    const muteBtn = h('button', { type: 'button', class: 'icon-btn mute-btn', title: 'Sound on/off', 'aria-label': 'Sound on or off', onclick: () => { this.toggleMute(); clear(muteBtn); muteBtn.append(icon(isMuted() ? 'mute' : 'sound')); } }, icon(isMuted() ? 'mute' : 'sound'));
    this.aiToggle = h('button', { type: 'button', class: 'btn btn-ai', onclick: () => this.toggleAI() }, icon('sparkle'), h('span', {}, 'AI helper'));
    const topbar = h('header', { class: 'topbar' },
      h('div', { class: 'brand' }, h('span', { class: 'brand-mark', html: LOGO }), h('span', { class: 'brand-name' }, 'Xordor')),
      h('div', { class: 'title-wrap' }, this.titleInput(), this.saveStateEl()),
      h('nav', { class: 'menu', 'aria-label': 'File' },
        this.topButton('New', 'file', () => this.command('new'), 'Start a new game'),
        this.topButton('Open', 'open', () => this.command('open'), 'Open a game file'),
        this.topButton('Save', 'save', () => this.command('save'), 'Save your game (Ctrl+S)'),
        this.topButton('Publish', 'publish', () => this.command('export'), 'Make a playable file to share')),
      this.levelSwitcher(),
      h('div', { class: 'top-actions' }, ...this.undoRedo(), muteBtn,
        h('button', { type: 'button', class: 'icon-btn', title: 'Help', 'aria-label': 'Help', onclick: () => this.help() }, icon('help')),
        this.modeToggle(), this.aiToggle, this.playControls()));

    const leftTabs = h('div', { class: 'tabs', role: 'tablist' });
    const addRoot = h('div', { class: 'panel-body', role: 'tabpanel' });
    const objectsRoot = h('div', { class: 'panel-body', role: 'tabpanel', hidden: true });
    this.setLeft = (which) => {
      addRoot.hidden = which !== 'add';
      objectsRoot.hidden = which !== 'objects';
      for (const b of leftTabs.children) b.setAttribute('aria-selected', b.dataset.tab === which ? 'true' : 'false');
      if (which === 'objects') this.objectsPanel.render();
    };
    leftTabs.append(
      h('button', { type: 'button', role: 'tab', class: 'tab', dataset: { tab: 'add' }, 'aria-selected': 'true', onclick: () => this.setLeft('add') }, icon('plus'), 'Add'),
      h('button', { type: 'button', role: 'tab', class: 'tab', dataset: { tab: 'objects' }, 'aria-selected': 'false', onclick: () => this.setLeft('objects') }, icon('list'), 'Objects'));
    const left = h('aside', { class: 'side side-left', 'aria-label': 'Add and objects' }, leftTabs, addRoot, objectsRoot);
    this.simpleRight = h('aside', { class: 'side side-right', 'aria-label': 'Properties' }, h('div', { class: 'tabs' }, h('span', { class: 'tab static' }, icon('sliders'), 'Properties')));
    this.simpleAI = h('aside', { class: 'side side-ai', 'aria-label': 'AI helper' });
    const mobileTab = (tab, ic, label) => h('button', { type: 'button', class: 'mtab', dataset: { tab }, onclick: () => this.setMobileTab(tab) }, icon(ic), h('span', {}, label));
    const mobileBar = h('nav', { class: 'mobile-tabs', 'aria-label': 'Panels' },
      mobileTab('add', 'plus', 'Add'), mobileTab('objects', 'list', 'Objects'), mobileTab('props', 'sliders', 'Edit'), mobileTab('ai', 'sparkle', 'AI'));
    this.addPanel = new AddPanel(addRoot, ed);
    this.objectsPanel = new ObjectsPanel(objectsRoot, ed);
    this.simpleParts = [topbar, left, 'viewport', this.simpleRight, this.simpleAI, mobileBar];
  }

  // ---------- Pro layout ----------
  buildProLayout() {
    const ed = this.editor;
    this.pauseBtn = h('button', { type: 'button', class: 'icon-btn', title: 'Pause (P while playing)', 'aria-label': 'Pause', disabled: true, onclick: () => { this.game?.setPaused(!this.game.paused); this.updatePlayButtons(); } }, icon('pause'));
    this.playHereBtn = h('button', { type: 'button', class: 'btn btn-ghost', title: 'Play from the camera position (Alt+Shift+P)', onclick: () => this.play({ fromCamera: true }) }, icon('focus'), h('span', { class: 'label' }, 'From camera'));
    const toolbar = h('header', { class: 'pro-toolbar' },
      h('div', { class: 'brand' }, h('span', { class: 'brand-mark', html: LOGO }), h('span', { class: 'brand-name' }, 'Xordor')),
      h('div', { class: 'title-wrap' }, this.titleInput(), this.saveStateEl()),
      h('div', { class: 'tb-group' },
        this.topButton('Save', 'save', () => this.command('save'), 'Save (Ctrl+S)'),
        this.topButton('Open', 'open', () => this.command('open'), 'Open project (Ctrl+O)'),
        this.topButton('Export', 'publish', () => this.command('export'), 'Export a playable game (Ctrl+E)'),
        ...this.undoRedo()),
      this.levelSwitcher(),
      h('div', { class: 'tb-group' },
        this.topButton('Settings', 'gear', () => this.projectSettings(), 'Project settings'),
        this.topButton('Content', 'open', () => this.showDock('content'), 'Content Browser')),
      h('span', { class: 'spacer' }),
      h('div', { class: 'tb-group play-group' }, this.playControls(), this.playHereBtn, this.pauseBtn),
      this.modeToggle());

    const placeRoot = h('div', { class: 'panel-body compact-tiles' });
    this.placePanel = new AddPanel(placeRoot, ed);
    const place = h('aside', { class: 'pro-panel pro-place', 'aria-label': 'Place actors' }, h('div', { class: 'pro-head' }, icon('plus'), 'Place Actors'), placeRoot);

    const outlinerRoot = h('div', { class: 'outliner' });
    this.outliner = new OutlinerPanel(outlinerRoot, ed, { onSavePrefab: () => this.savePrefab() });
    this.proDetails = h('div', { class: 'pro-details' }, h('div', { class: 'pro-head' }, icon('sliders'), 'Details'));
    const right = h('aside', { class: 'pro-panel pro-right', 'aria-label': 'Outliner and details' },
      h('div', { class: 'pro-outliner' }, h('div', { class: 'pro-head' }, icon('list'), 'Outliner'), outlinerRoot),
      this.proDetails);

    // Bottom dock with tabs.
    const contentRoot = h('div', { class: 'dock-pane content-browser' });
    const logRoot = h('div', { class: 'dock-pane output-log' });
    const codeRoot = h('div', { class: 'dock-pane code-pane' });
    this.proAI = h('div', { class: 'dock-pane ai-pane' });
    this.dockPanes = { content: contentRoot, log: logRoot, code: codeRoot, ai: this.proAI };
    this.contentBrowser = new ContentBrowser(contentRoot, ed, { onOpenCode: (t) => this.openCode(t) });
    this.outputLog = new OutputLog(logRoot);
    this.codePanel = new CodePanel(codeRoot, ed, { onAskAI: (text) => this.openAI(text) });
    this.dockTabs = h('div', { class: 'dock-tabs', role: 'tablist' });
    this.errorBadge = h('span', { class: 'badge danger', hidden: true }, '!');
    for (const [key, label, ic] of [['content', 'Content Browser', 'open'], ['log', 'Output Log', 'list'], ['code', 'Code', 'file'], ['ai', 'AI Assistant', 'sparkle']]) {
      this.dockTabs.append(h('button', { type: 'button', role: 'tab', class: 'dock-tab', dataset: { pane: key }, onclick: () => this.showDock(key) }, icon(ic), label, key === 'log' ? this.errorBadge : null));
    }
    this.outputLog.onEntry = (e) => { if (e.level === 'error' && this.dock !== 'log') this.errorBadge.hidden = false; };
    const dock = h('section', { class: 'pro-dock', 'aria-label': 'Content, log, code and AI' }, this.dockTabs, h('div', { class: 'dock-body' }, contentRoot, logRoot, codeRoot, this.proAI));
    this.proParts = [toolbar, place, 'viewport', right, dock];
    this.showDock(pref.get('dock', 'content'));
  }

  showDock(key) {
    if (!this.dockPanes[key]) key = 'content';
    this.dock = key;
    pref.set('dock', key);
    for (const [k, el] of Object.entries(this.dockPanes)) el.hidden = k !== key;
    for (const b of this.dockTabs.children) b.setAttribute('aria-selected', b.dataset.pane === key ? 'true' : 'false');
    if (key === 'log') this.errorBadge.hidden = true;
    if (key === 'content') this.contentBrowser.render();
    if (key === 'code') this.codePanel.refresh();
    if (key === 'log') this.outputLog.list.scrollTop = this.outputLog.list.scrollHeight;
  }

  applyMode(mode) {
    if (mode === 'pro' && narrow()) {
      toast('Pro mode needs a wider window.');
      mode = 'simple';
    }
    this.mode = mode;
    pref.set('mode', mode);
    const root = this.root;
    clear(root);
    root.className = `studio mode-${mode}${pref.get('aiHidden') === '1' ? ' ai-hidden' : ''}${this.game ? ' playing' : ''}`;
    root.dataset.tab = 'add';
    const parts = mode === 'pro' ? this.proParts : this.simpleParts;
    for (const part of parts) root.append(part === 'viewport' ? this.viewport : part);
    if (mode === 'pro') {
      this.proDetails.append(this.inspectorRoot);
      this.proAI.append(this.aiRoot);
    } else {
      this.simpleRight.append(this.inspectorRoot);
      this.simpleAI.append(this.aiRoot);
    }
    document.documentElement.dataset.mode = mode;
    this.editor.setNavigation(mode === 'pro' ? 'unreal' : 'orbit');
    this.inspector.setPro(mode === 'pro');
    for (const t of this.modeToggles || []) for (const b of t.children) b.setAttribute('aria-checked', b.dataset.mode === mode ? 'true' : 'false');
    this.addPanel.filter?.();
    this.objectsPanel.render();
    this.outliner.render();
    this.contentBrowser.render();
    this.updateChrome();
    this.updateTools();
    this.updatePlayButtons();
    desktop?.window.setMode(mode);
    requestAnimationFrame(() => this.resize());
  }

  setMobileTab(tab) {
    this.root.dataset.tab = tab;
    if (tab === 'add' || tab === 'objects') this.setLeft?.(tab);
    for (const b of this.root.querySelectorAll('.mtab')) b.setAttribute('aria-current', b.dataset.tab === tab ? 'page' : 'false');
    requestAnimationFrame(() => this.resize());
  }

  toggleAI() {
    if (this.mode === 'pro') { this.showDock(this.dock === 'ai' ? 'content' : 'ai'); if (this.dock === 'ai') this.ai.focus(); return; }
    const hidden = this.root.classList.toggle('ai-hidden');
    pref.set('aiHidden', hidden ? '1' : '0');
    if (!hidden) this.ai.focus();
    requestAnimationFrame(() => this.resize());
  }

  openAI(text = '') {
    if (this.mode === 'pro') this.showDock('ai');
    else {
      this.root.classList.remove('ai-hidden');
      pref.set('aiHidden', '0');
      this.setMobileTab('ai');
    }
    this.ai.ask(text);
  }

  openCode(target) {
    if (this.mode !== 'pro') this.applyMode('pro');
    this.showDock('code');
    this.codePanel.open(target);
  }

  async savePrefab() {
    const ed = this.editor;
    const ents = ed.selectedEntities.filter((e) => e.shape !== 'spawn');
    if (!ents.length) { toast('Select one or more objects first.'); return; }
    const name = await askText('Save as prefab', { label: 'Prefab name', value: ents.length === 1 ? ents[0].name : `${ents[0].name} group`, ok: 'Save prefab' });
    if (name === null) return;
    const prefab = ed.saveSelectionAsPrefab(name);
    if (prefab) {
      toast(`Saved "${prefab.name}". Find it in the Content Browser under My prefabs.`);
      if (this.mode === 'pro') { this.showDock('content'); this.contentBrowser.setSection('prefabs'); }
    }
  }

  toggleMute() {
    setMuted(!isMuted());
    pref.set('muted', isMuted() ? '1' : '0');
  }

  // ---------- commands (buttons, keyboard and desktop menus) ----------
  command(cmd, arg) {
    const ed = this.editor;
    const typing = isTyping(document.activeElement);
    switch (cmd) {
      case 'new': return this.startScreen();
      case 'open': return this.openFile();
      case 'open-path': return this.openPath(arg);
      case 'save': return this.save();
      case 'save-as': return this.save({ as: true });
      case 'export': return this.publish();
      case 'undo': if (typing) document.execCommand('undo'); else if (!this.game) ed.undo(); return null;
      case 'redo': if (typing) document.execCommand('redo'); else if (!this.game) ed.redo(); return null;
      case 'duplicate': if (!typing && !this.game) ed.duplicateSelected(); return null;
      case 'delete': if (!typing && !this.game) ed.deleteSelected(); return null;
      case 'select-all': if (typing) document.execCommand('selectAll'); else if (!this.game) ed.selectAll(); return null;
      case 'project-settings': return this.projectSettings();
      case 'ai-settings': return this.ai.openSettings();
      case 'mode': return this.applyMode(arg);
      case 'toggle-ai': return this.toggleAI();
      case 'focus': if (!typing) ed.focusSelected(); return null;
      case 'play': return this.game ? this.stopPlaying() : this.play();
      case 'play-here': return this.play({ fromCamera: true });
      case 'stop': return this.stopPlaying();
      case 'help': return this.help();
      case 'shortcuts': return this.shortcuts();
      case 'script-help': return this.scriptHelp();
      default: return null;
    }
  }

  bindKeys() {
    window.addEventListener('keydown', (e) => {
      if (this.game || this.editor.flying) return;
      if (document.querySelector('.dialog-backdrop')) return;
      const typing = isTyping(e.target);
      const mod = e.ctrlKey || e.metaKey;
      const key = e.key.toLowerCase();
      // In the desktop app the menu bar handles these shortcuts.
      if (!isDesktop) {
        if (mod && key === 's') { e.preventDefault(); this.save(); return; }
        if (mod && key === 'o') { e.preventDefault(); this.openFile(); return; }
        if (e.altKey && key === 'p') { e.preventDefault(); this.play({ fromCamera: e.shiftKey }); return; }
        if (typing) return;
        if (mod && key === 'z') { e.preventDefault(); if (e.shiftKey) this.editor.redo(); else this.editor.undo(); return; }
        if (mod && key === 'y') { e.preventDefault(); this.editor.redo(); return; }
        if (mod && key === 'd') { e.preventDefault(); this.editor.duplicateSelected(); return; }
        if (mod && key === 'a') { e.preventDefault(); this.editor.selectAll(); return; }
      }
      if (typing || mod || e.altKey) return;
      const ed = this.editor;
      switch (e.key) {
        case 'Delete':
        case 'Backspace': if (ed.selection.length) { e.preventDefault(); ed.deleteSelected(); } break;
        case 'w': case 'W': ed.setGizmoMode('translate'); break;
        case 'e': case 'E': ed.setGizmoMode('rotate'); break;
        case 'r': case 'R': ed.setGizmoMode('scale'); break;
        case ' ': if (this.mode === 'pro' && e.target === this.canvas) { e.preventDefault(); ed.cycleGizmoMode(); } break;
        case 'f': case 'F': ed.focusSelected(); break;
        case 'End': ed.dropSelectedToFloor(); break;
        case 'F2': if (this.mode === 'pro' && ed.selected) this.outliner.rename(ed.selected); break;
        case 'p': case 'P': if (this.mode === 'simple') this.play(); break;
        case 'Escape': ed.select(null); break;
        default:
      }
    });
  }

  bindDesktop() {
    if (!isDesktop) return;
    desktop.window.onMenu(({ command, arg }) => this.command(command, arg));
    desktop.window.onSaveAndClose(async () => {
      if (await this.save()) desktop.window.closeReady();
    });
  }

  // ---------- play mode ----------
  play({ fromCamera = false } = {}) {
    if (this.game) return;
    const ed = this.editor;
    this.codePanel?.flush();
    this.root.classList.add('playing');
    ed.setActive(false);
    const spawnAt = fromCamera ? ed.cameraSpawn() : null;
    const start = (levelId, carry = null, autoBegin = false) => {
      this.game?.dispose();
      this.game = new Game({ container: this.viewport, canvas: this.canvas, project: ed.doc, levelId, carry, spawnAt: levelId === ed.levelId ? spawnAt : null, onExit: () => this.stopPlaying() });
      this.game.onRestart = () => start(ed.levelId);
      this.game.onNextLevel = (next, c) => start(next, c, true);
      this.game.onPauseChange = () => this.updatePlayButtons();
      this.game.onScriptError = (info) => { this.codePanel?.showRuntimeError(info); };
      if (autoBegin || fromCamera) this.game.begin();
      this.resize();
      this.updatePlayButtons();
    };
    start(ed.levelId);
    if (this.mode === 'simple') this.setMobileTab('play');
  }

  stopPlaying() {
    if (!this.game) return;
    this.game.dispose();
    this.game = null;
    this.root.classList.remove('playing');
    this.editor.setActive(true);
    if (this.mode === 'simple') this.setMobileTab('add');
    this.updatePlayButtons();
    this.resize();
    this.canvas.focus();
    logger.info('game', 'Stopped playing.');
  }

  // ---------- files ----------
  projectText() {
    return projectToFile(this.editor.doc);
  }

  async confirmDiscard() {
    if (!isDesktop || !this.editor.dirty) return true;
    return confirmDialog('Unsaved changes', 'Your current project has changes that are not saved to a file. Continue anyway?', { ok: 'Continue', danger: true });
  }

  async loadProject(project, filePath = null) {
    if (hasScripts(project)) {
      const keep = await confirmDialog('This project has custom code',
        'This project contains code that runs while playing. Only keep it if you trust the person who made it.',
        { ok: 'Keep the code', cancel: 'Remove the code', danger: true });
      if (!keep) stripScripts(project);
    }
    this.stopPlaying();
    this.filePath = filePath;
    this.editor.load(project);
    if (filePath) this.editor.markSaved();
    this.editor.focusSelected();
    autosave(this.editor.doc, { filePath, levelId: this.editor.levelId });
    this.updateChrome();
    logger.info('editor', `Opened "${project.name}"${filePath ? ` from ${filePath}` : ''}.`);
  }

  async openFile() {
    if (!(await this.confirmDiscard())) return;
    let text;
    let path = null;
    if (isDesktop) {
      const res = await desktop.project.open();
      if (!res) return;
      ({ text, path } = res);
    } else {
      const file = await pickFile('.xordor,.json,.html,application/json,text/html');
      if (!file) return;
      text = await file.text();
    }
    try {
      const project = parseProjectFile(text);
      await this.loadProject(project, path);
      toast(`Opened "${project.name}"`);
    } catch (err) {
      toast(`Couldn't open that file. ${err.message}`, { kind: 'error', ms: 5000 });
    }
  }

  async openPath(file) {
    if (!isDesktop || !(await this.confirmDiscard())) return;
    try {
      const res = await desktop.project.openPath(file);
      if (!res) { toast('That project file is no longer there.', { kind: 'error' }); return; }
      await this.loadProject(parseProjectFile(res.text), res.path);
    } catch (err) {
      toast(`Couldn't open that file. ${err.message}`, { kind: 'error', ms: 5000 });
    }
  }

  /** Saves the project. Returns true when it was saved. */
  async save({ as = false } = {}) {
    this.codePanel?.flush();
    const p = this.editor.doc;
    if (isDesktop) {
      try {
        const path = this.filePath && !as ? await desktop.project.save(this.filePath, this.projectText()) : await desktop.project.saveAs(this.projectText(), p.name);
        if (!path) return false;
        this.filePath = path;
        this.editor.markSaved();
        autosave(p, { filePath: path, levelId: this.editor.levelId });
        toast(`Saved ${path.split(/[\\/]/).pop()}`);
        logger.success('editor', `Saved to ${path}`);
        return true;
      } catch (err) {
        toast(`Couldn't save. ${err.message}`, { kind: 'error', ms: 6000 });
        return false;
      }
    }
    download(fileNameFor(p.name, '.xordor'), this.projectText(), 'application/json');
    toast(`Downloading ${fileNameFor(p.name, '.xordor')}`);
    return true;
  }

  async publish() {
    this.codePanel?.flush();
    const p = this.editor.doc;
    if (!canPublish()) {
      openDialog({
        title: 'Export a playable game',
        body: h('div', {},
          h('p', {}, 'Exporting makes one HTML file that anyone can open to play your game.'),
          h('p', { class: 'muted' }, 'Xordor is running in developer mode, which cannot package itself. Start it with "npm start" to export games.')),
        actions: [{ label: 'OK', primary: true }],
      });
      return;
    }
    const name = fileNameFor(p.name, '.html');
    openDialog({
      title: 'Export a playable game',
      body: h('div', { class: 'publish' },
        h('p', {}, 'Xordor makes one file, ', h('strong', {}, name), `, with your whole game inside${p.levels.length > 1 ? ` (all ${p.levels.length} levels)` : ''}.`),
        h('ul', { class: 'steps' },
          h('li', {}, 'Anyone can double-click it to play. No install and no internet needed.'),
          h('li', {}, 'Share it by email or chat, or upload it to a game site such as itch.io (as an HTML game).'),
          h('li', {}, 'Your AI keys are never included.'))),
      actions: [
        { label: 'Cancel', value: null },
        {
          label: 'Export game', primary: true, run: async () => {
            const html = buildStandaloneHtml(p);
            if (!html) { toast('Exporting is not available here.', { kind: 'error' }); return false; }
            if (isDesktop) {
              const path = await desktop.project.exportGame(html, p.name);
              if (path) { toast(`Exported ${path.split(/[\\/]/).pop()}`); logger.success('editor', `Exported game to ${path}`); }
              return true;
            }
            download(name, html, 'text/html');
            toast(`Downloading ${name}`);
            return true;
          },
        },
      ],
    });
  }

  // ---------- dialogs ----------
  chooseMode(first = false) {
    const card = (mode, title, blurb, points) => h('button', {
      type: 'button', class: `mode-card mode-card-${mode}`, onclick: () => { d.close(); this.applyMode(mode); if (first) this.startScreen(); },
    }, h('span', { class: 'mode-card-art' }), h('strong', {}, title), h('span', { class: 'muted' }, blurb), h('ul', {}, points.map((p) => h('li', {}, p))));
    const d = openDialog({
      title: first ? 'Welcome to Xordor' : 'Choose how you work',
      wide: true,
      dismissible: !first,
      body: h('div', {},
        h('p', { class: 'lead' }, 'Xordor has two ways to work on the same games. You can switch at any time.'),
        h('div', { class: 'mode-grid' },
          card('simple', 'Simple', 'For everyone. No game-making experience needed.', ['Big friendly buttons and plain words', 'Click to add, drag to move', 'An AI helper that builds for you']),
          card('pro', 'Pro', 'For developers who know engines like Unreal or Unity.', ['Outliner, Details, Content Browser, Output Log', 'Code editor, levels, prefabs, multi-select', 'Fly camera, snapping, wireframe and stats']))),
    });
  }

  async startScreen() {
    if (!(await this.confirmDiscard())) return;
    const ed = this.editor;
    const recent = isDesktop ? await desktop.project.recent().catch(() => []) : [];
    const pick = async (key) => {
      d.close();
      this.stopPlaying();
      this.filePath = null;
      ed.load(buildTemplate(key));
      ed.dirty = true;
      ed.focusSelected();
      this.updateChrome();
      toast(`Started "${TEMPLATES.find((t) => t.key === key).label}"`);
    };
    const grid = h('div', { class: 'template-grid' }, TEMPLATES.map((t) => h('button', { type: 'button', class: 'template-card', onclick: () => pick(t.key) },
      h('span', { class: `template-art art-${t.key}` }), h('strong', {}, t.label), h('span', { class: 'muted small' }, t.blurb))));
    const side = h('div', { class: 'start-side' },
      h('h3', {}, 'Open'),
      h('button', { type: 'button', class: 'btn', onclick: () => { d.close(); this.openFile(); } }, icon('open'), 'Open a project file…'),
      h('button', { type: 'button', class: 'btn btn-ghost', onclick: () => d.close() }, icon('check'), `Keep working on "${ed.project.name}"`),
      recent.length ? h('h3', {}, 'Recent projects') : null,
      recent.length ? h('ul', { class: 'recent-list' }, recent.map((p) => h('li', {}, h('button', { type: 'button', class: 'recent-item', title: p, onclick: () => { d.close(); this.openPath(p); } },
        h('strong', {}, p.split(/[\\/]/).pop()), h('span', { class: 'muted small' }, p))))) : null);
    const d = openDialog({
      title: 'Start a project',
      wide: true,
      body: h('div', { class: 'start-screen' },
        h('div', {}, h('p', { class: 'lead' }, this.mode === 'pro' ? 'Pick a template or open a project.' : 'Pick a starting point. You can change anything, then press Play.'), grid),
        side),
    });
  }

  projectSettings() {
    const ed = this.editor;
    const s = ed.project.settings;
    const set = (patch) => ed.setSettings(patch, 'settings');
    const startSel = h('select', { class: 'input', id: 'start-level' }, ed.doc.levels.map((l) => h('option', { value: l.id, selected: l.id === ed.doc.startLevel }, l.name)));
    startSel.addEventListener('change', () => ed.setStartLevel(startSel.value));
    openDialog({
      title: 'Project settings',
      wide: true,
      body: h('div', { class: 'settings-grid' },
        h('section', {}, h('h3', {}, 'Game'),
          textField('Game name', ed.project.name, (v) => ed.rename(v)),
          h('div', { class: 'field' }, h('label', { class: 'field-label', for: 'start-level' }, 'Play starts at level'), startSel),
          selectField('Camera', s.camera, CAMERAS, (v) => set({ camera: v })),
          selectField('How to win a level', s.winCondition, WIN_CONDITIONS, (v) => set({ winCondition: v })),
          numberField('Points needed (score rule)', s.targetScore, (v) => set({ targetScore: v }), { min: 1, step: 1 }),
          numberField('Lives (0 = unlimited)', s.lives, (v) => set({ lives: v }), { min: 0, max: 99, step: 1 }),
          numberField('Time limit per level, seconds (0 = none)', s.timeLimit, (v) => set({ timeLimit: v }), { min: 0, step: 5 }),
          textField('Message before playing', s.startMessage, (v) => set({ startMessage: v }))),
        h('section', {}, h('h3', {}, 'Player and physics'),
          colorField('Player color', s.playerColor, (v) => set({ playerColor: v })),
          rangeField('Running speed (m/s)', s.playerSpeed, (v) => set({ playerSpeed: v }), { min: 1, max: 30, step: 0.5, format: (v) => v.toFixed(1) }),
          rangeField('Jump power', s.jumpPower, (v) => set({ jumpPower: v }), { min: 0, max: 30, step: 0.5, format: (v) => v.toFixed(1) }),
          rangeField('Gravity (m/s²)', s.gravity, (v) => set({ gravity: v }), { min: 1, max: 60, step: 1, format: (v) => String(v) }),
          numberField('Fall out of the world below height', s.fallLimit, (v) => set({ fallLimit: v }), { step: 1 }),
          checkField('Sound effects', !isMuted(), (v) => { if (v === isMuted()) this.toggleMute(); }))),
      actions: [{ label: 'Done', primary: true }],
    });
  }

  help() {
    const pro = this.mode === 'pro';
    openDialog({
      title: 'How to use Xordor',
      wide: true,
      body: h('div', { class: 'help' },
        h('div', { class: 'help-cols' },
          h('section', {},
            h('h3', {}, 'Building'),
            h('ul', {},
              h('li', {}, pro ? 'Click an item in Place Actors to add it in front of the camera.' : 'Click a thing in the Add panel to drop it in front of you.'),
              h('li', {}, 'Click an object to select it. Drag the colored arrows to move it. Ctrl+click selects more than one.'),
              h('li', {}, h('kbd', {}, 'W'), ' move, ', h('kbd', {}, 'E'), ' rotate, ', h('kbd', {}, 'R'), ' scale, ', h('kbd', {}, 'F'), ' zoom to it, ', h('kbd', {}, 'End'), ' drop to floor'),
              h('li', {}, pro ? 'Hold the right mouse button and use W A S D Q E to fly. Scroll while flying to change speed.' : 'Drag empty space to look around, right-drag to slide, scroll to zoom.'),
              h('li', {}, 'Click empty space to see the world settings: sky, ground and game rules.'))),
          h('section', {},
            h('h3', {}, 'Playing'),
            h('ul', {},
              h('li', {}, h('kbd', {}, 'W'), h('kbd', {}, 'A'), h('kbd', {}, 'S'), h('kbd', {}, 'D'), ' to move, ', h('kbd', {}, 'Space'), ' to jump, ', h('kbd', {}, 'Shift'), ' to run'),
              h('li', {}, 'Drag the mouse to look. ', h('kbd', {}, 'P'), ' pauses, ', h('kbd', {}, 'Esc'), ' goes back to the editor.'),
              h('li', {}, 'Games can have several levels. Winning one moves on to the next.'))),
          h('section', {},
            h('h3', {}, 'AI helper'),
            h('ul', {},
              h('li', {}, 'Type what you want, like "add a bridge made of crates" or "make enemies faster".'),
              h('li', {}, 'Select an object first to change just that object.'),
              h('li', {}, 'Connect ChatGPT, Claude, Gemini, OpenRouter or a free local model with Ollama in AI settings.'),
              h('li', {}, 'Every AI change can be undone with one click.'))))),
      actions: [{ label: 'Keyboard shortcuts', run: () => { this.shortcuts(); return true; } }, { label: 'Got it', primary: true }],
    });
  }

  shortcuts() {
    const rows = [
      ['W / E / R', 'Move, rotate, scale tool'], ['Space (Pro)', 'Cycle the tool'], ['F', 'Zoom to selection'], ['End', 'Drop selection to the floor'],
      ['Ctrl+D', 'Duplicate'], ['Delete', 'Delete'], ['Ctrl+A', 'Select all'], ['Ctrl+click', 'Add to selection'], ['F2 (Pro)', 'Rename'],
      ['Ctrl+Z / Ctrl+Y', 'Undo / redo'], ['Ctrl+S / Ctrl+Shift+S', 'Save / save as'], ['Ctrl+O', 'Open project'], ['Ctrl+E', 'Export a playable game'],
      ['Alt+P', 'Play'], ['Alt+Shift+P', 'Play from the camera'], ['P', 'Pause while playing'], ['Esc', 'Stop playing'],
      ['Right mouse + W A S D Q E (Pro)', 'Fly the camera'], ['Ctrl+1 / Ctrl+2', 'Simple / Pro mode'], ['Ctrl+J', 'AI helper'],
    ];
    openDialog({
      title: 'Keyboard shortcuts',
      wide: true,
      body: h('table', { class: 'shortcut-table' }, h('tbody', {}, rows.map(([k, v]) => h('tr', {}, h('td', {}, h('kbd', {}, k)), h('td', {}, v))))),
      actions: [{ label: 'Close', primary: true }],
    });
  }

  scriptHelp() {
    openDialog({
      title: 'Script reference',
      wide: true,
      body: h('pre', { class: 'code-ref dialog-ref' }, SCRIPT_REFERENCE),
      actions: [{ label: 'Close', primary: true }],
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
