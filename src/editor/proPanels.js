// Pro mode panels, laid out like a big engine: Outliner, Content Browser,
// Output Log, Code editor, the viewport toolbar and the stats overlay.
import { clear, confirmDialog, h, icon, openDialog, toast } from '../ui/dom.js';
import { SHAPES } from '../core/shapes.js';
import { PREFAB_GROUPS } from '../core/prefabs.js';
import { BEHAVIORS } from '../core/behaviors.js';
import { clearLog, logEntries, logger, onLog, scriptErrorLine } from '../core/log.js';
import { renderThumbnails } from './thumbnails.js';
import { createCodeEditor } from './codeEditor.js';
import { TEMPLATES, templateActions } from '../templates.js';
import { applyActions } from '../ai/actions.js';

/** Asks for a line of text inside the app (window.prompt is not available everywhere). */
export function askText(title, { label = 'Name', value = '', ok = 'OK' } = {}) {
  return new Promise((resolve) => {
    const input = h('input', { type: 'text', class: 'input', value, 'aria-label': label });
    let result = null;
    const d = openDialog({
      title,
      body: h('div', { class: 'field' }, h('label', { class: 'field-label' }, label), input),
      actions: [{ label: 'Cancel', value: null }, { label: ok, primary: true, run: () => { result = input.value.trim(); return true; } }],
      onClose: () => resolve(result),
    });
    input.addEventListener('keydown', (e) => { if (e.key === 'Enter') { result = input.value.trim(); d.close(result); } });
    setTimeout(() => { input.focus(); input.select(); }, 40);
  });
}

// ---------------------------------------------------------------- context menu
let openMenu = null;
export function contextMenu(x, y, items) {
  openMenu?.remove();
  const menu = h('div', { class: 'context-menu', role: 'menu' }, items.filter(Boolean).map((it) => (it === '-'
    ? h('div', { class: 'menu-sep' })
    : h('button', { type: 'button', role: 'menuitem', class: `menu-item ${it.danger ? 'danger' : ''}`, disabled: it.disabled, onclick: () => { close(); it.run(); } },
      it.icon ? icon(it.icon) : h('span', { class: 'icon' }), h('span', {}, it.label), it.hint ? h('kbd', {}, it.hint) : null))));
  const close = () => { menu.remove(); openMenu = null; document.removeEventListener('pointerdown', outside, true); };
  const outside = (e) => { if (!menu.contains(e.target)) close(); };
  document.body.append(menu);
  const r = menu.getBoundingClientRect();
  menu.style.left = `${Math.min(x, window.innerWidth - r.width - 8)}px`;
  menu.style.top = `${Math.min(y, window.innerHeight - r.height - 8)}px`;
  setTimeout(() => document.addEventListener('pointerdown', outside, true));
  openMenu = menu;
}

// ---------------------------------------------------------------- Outliner
export class OutlinerPanel {
  constructor(root, editor, { onSavePrefab } = {}) {
    this.root = root;
    this.editor = editor;
    this.onSavePrefab = onSavePrefab;
    this.query = '';
    this.collapsed = new Set();
    this.order = [];
    const search = h('input', { type: 'search', class: 'input search', placeholder: 'Search actors', 'aria-label': 'Search actors' });
    search.addEventListener('input', () => { this.query = search.value.trim().toLowerCase(); this.render(); });
    this.count = h('span', { class: 'muted small' });
    this.tree = h('div', { class: 'tree', role: 'tree', 'aria-label': 'Outliner', tabIndex: 0 });
    root.append(h('div', { class: 'panel-pad outliner-bar' }, search, this.count), h('div', { class: 'tree-head' }, h('span', {}, 'Label'), h('span', {}, 'Type'), h('span', {})), this.tree);
    editor.on('change', () => this.render());
    editor.on('select', () => this.render());
  }

  render() {
    const p = this.editor.project;
    if (!p || !this.root.isConnected) return;
    const scroll = this.tree.scrollTop;
    clear(this.tree);
    const items = p.objects.filter((o) => !this.query || o.name.toLowerCase().includes(this.query) || o.shape.includes(this.query) || o.tags.some((t) => t.toLowerCase().includes(this.query)) || o.folder.toLowerCase().includes(this.query));
    this.count.textContent = `${p.objects.length} actors`;
    // Build a folder tree.
    const rootNode = { name: '', path: '', folders: new Map(), items: [] };
    for (const o of items) {
      let node = rootNode;
      if (o.folder) {
        for (const part of o.folder.split('/')) {
          const path = node.path ? `${node.path}/${part}` : part;
          if (!node.folders.has(part)) node.folders.set(part, { name: part, path, folders: new Map(), items: [] });
          node = node.folders.get(part);
        }
      }
      node.items.push(o);
    }
    this.order = [];
    const sel = new Set(this.editor.selection);
    const draw = (node, depth) => {
      for (const f of [...node.folders.values()].sort((a, b) => a.name.localeCompare(b.name))) {
        const closed = this.collapsed.has(f.path) && !this.query;
        const total = countItems(f);
        this.tree.append(h('div', {
          class: 'tree-row folder', role: 'treeitem', 'aria-expanded': closed ? 'false' : 'true', style: { paddingLeft: `${8 + depth * 14}px` },
          onclick: () => { if (closed) this.collapsed.delete(f.path); else this.collapsed.add(f.path); this.render(); },
          oncontextmenu: (e) => { e.preventDefault(); this.folderMenu(e, f); },
        }, h('span', { class: `caret ${closed ? '' : 'open'}` }, '▸'), h('span', { class: 'folder-icon' }), h('span', { class: 'tree-label' }, f.name), h('span', { class: 'muted small' }, String(total)), h('span', {})));
        if (!closed) draw(f, depth + 1);
      }
      for (const o of node.items) {
        this.order.push(o.id);
        const row = h('div', {
          class: `tree-row ${sel.has(o.id) ? 'selected' : ''} ${o.id === this.editor.selectedId ? 'primary' : ''}`, role: 'treeitem', 'aria-selected': sel.has(o.id) ? 'true' : 'false',
          style: { paddingLeft: `${8 + depth * 14 + 14}px` },
          onclick: (e) => this.clickRow(e, o.id),
          ondblclick: () => { this.editor.select(o.id); this.editor.focusSelected(); },
          oncontextmenu: (e) => { e.preventDefault(); if (!sel.has(o.id)) this.editor.select(o.id); this.rowMenu(e, o); },
        },
        h('span', { class: 'dot', style: { background: o.color } }),
        h('span', { class: `tree-label ${o.visible ? '' : 'muted'}` }, o.name, o.behaviors.some((b) => b.type === 'script') ? h('span', { class: 'badge' }, 'JS') : null),
        h('span', { class: 'muted small type' }, SHAPES[o.shape]?.label || o.shape),
        h('button', {
          type: 'button', class: 'icon-btn small', 'aria-label': o.visible ? `Hide ${o.name}` : `Show ${o.name}`,
          onclick: (e) => { e.stopPropagation(); this.editor.updateEntity(o.id, { visible: !o.visible }, 'outliner'); },
        }, icon(o.visible ? 'eye' : 'eyeOff')));
        this.tree.append(row);
      }
    };
    draw(rootNode, 0);
    if (!items.length) this.tree.append(h('p', { class: 'muted small panel-pad' }, this.query ? 'No actors match.' : 'This level is empty. Place actors from the left panel.'));
    this.tree.scrollTop = scroll;
  }

  clickRow(e, id) {
    const ed = this.editor;
    if (e.shiftKey && ed.selectedId) {
      const a = this.order.indexOf(ed.selectedId);
      const b = this.order.indexOf(id);
      if (a >= 0 && b >= 0) {
        const [lo, hi] = a < b ? [a, b] : [b, a];
        ed.selectMany([...new Set([...ed.selection, ...this.order.slice(lo, hi + 1)])]);
        return;
      }
    }
    ed.select(id, { toggle: e.ctrlKey || e.metaKey });
  }

  async rename(o) {
    const name = await askText('Rename actor', { value: o.name, ok: 'Rename' });
    if (name) this.editor.updateEntity(o.id, { name }, 'outliner');
  }

  async moveToFolder() {
    const ed = this.editor;
    const current = ed.selected?.folder || '';
    const folder = await askText('Move to folder', { label: 'Folder (use / for subfolders, empty for none)', value: current, ok: 'Move' });
    if (folder === null && current === '') return;
    ed.updateEntities(ed.selection, () => ({ folder: folder || '' }), 'outliner', 'Move to folder');
  }

  rowMenu(e, o) {
    const ed = this.editor;
    const many = ed.selection.length > 1;
    contextMenu(e.clientX, e.clientY, [
      { label: 'Zoom to', icon: 'focus', hint: 'F', run: () => ed.focusSelected() },
      !many && { label: 'Rename…', icon: 'file', hint: 'F2', run: () => this.rename(o) },
      { label: many ? `Duplicate ${ed.selection.length}` : 'Duplicate', icon: 'copy', hint: 'Ctrl+D', run: () => ed.duplicateSelected() },
      { label: 'Snap to floor', icon: 'ground', hint: 'End', run: () => ed.dropSelectedToFloor() },
      { label: 'Move to folder…', icon: 'open', run: () => this.moveToFolder() },
      { label: 'Save as prefab…', icon: 'cube', run: () => this.onSavePrefab?.() },
      '-',
      { label: many ? `Delete ${ed.selection.length}` : 'Delete', icon: 'trash', hint: 'Del', danger: true, run: () => ed.deleteSelected() },
    ]);
  }

  folderMenu(e, f) {
    const ed = this.editor;
    const ids = collectIds(f);
    contextMenu(e.clientX, e.clientY, [
      { label: 'Select everything in folder', icon: 'list', run: () => ed.selectMany(ids) },
      { label: 'Rename folder…', icon: 'file', run: async () => {
        const name = await askText('Rename folder', { value: f.path, ok: 'Rename' });
        if (name === null) return;
        ed.updateEntities(ids, (o) => ({ folder: name + o.folder.slice(f.path.length) }), 'outliner', 'Rename folder');
      } },
      { label: 'Remove folder (keep actors)', icon: 'close', run: () => ed.updateEntities(ids, (o) => ({ folder: o.folder.slice(f.path.length).replace(/^\//, '') }), 'outliner', 'Remove folder') },
    ]);
  }
}

function countItems(node) {
  let n = node.items.length;
  for (const f of node.folders.values()) n += countItems(f);
  return n;
}

function collectIds(node) {
  return [...node.items.map((o) => o.id), ...[...node.folders.values()].flatMap(collectIds)];
}

// ---------------------------------------------------------------- Content Browser
export class ContentBrowser {
  constructor(root, editor, { onOpenCode } = {}) {
    this.root = root;
    this.editor = editor;
    this.onOpenCode = onOpenCode;
    this.section = 'levels';
    this.nav = h('nav', { class: 'cb-nav', 'aria-label': 'Content' });
    this.body = h('div', { class: 'cb-body' });
    root.append(this.nav, this.body);
    editor.on('change', () => this.render());
    editor.on('level', () => this.render());
    this.render();
  }

  setSection(s) {
    this.section = s;
    this.render();
  }

  render() {
    if (!this.editor.doc || !this.root.isConnected) return;
    const doc = this.editor.doc;
    const sections = [
      ['levels', 'Levels', doc.levels.length],
      ['prefabs', 'My prefabs', doc.prefabs.length],
      ['library', 'Built-in library', ''],
      ['scripts', 'Scripts', ''],
    ];
    clear(this.nav);
    for (const [key, label, n] of sections) {
      this.nav.append(h('button', { type: 'button', class: `cb-nav-item ${this.section === key ? 'on' : ''}`, onclick: () => this.setSection(key) }, h('span', { class: 'folder-icon' }), label, n !== '' ? h('span', { class: 'muted small' }, String(n)) : null));
    }
    clear(this.body);
    if (this.section === 'levels') this.renderLevels(doc);
    else if (this.section === 'prefabs') this.renderPrefabs(doc);
    else if (this.section === 'library') this.renderLibrary();
    else this.renderScripts();
  }

  renderLevels(doc) {
    const ed = this.editor;
    const bar = h('div', { class: 'cb-bar' },
      h('button', { type: 'button', class: 'btn btn-small btn-primary', onclick: () => this.newLevel() }, icon('plus'), 'New level'),
      h('span', { class: 'muted small' }, 'Levels play in this order. Winning a level moves on to the next one.'));
    const grid = h('div', { class: 'cb-grid' }, doc.levels.map((l, i) => {
      const isOpen = l.id === ed.levelId;
      const isStart = l.id === doc.startLevel;
      const env = l.environment;
      return h('div', {
        class: `asset-card ${isOpen ? 'on' : ''}`,
        ondblclick: () => ed.switchLevel(l.id),
        oncontextmenu: (e) => { e.preventDefault(); this.levelMenu(e, l, i); },
      },
      h('button', { type: 'button', class: 'asset-thumb', style: { background: `linear-gradient(180deg, ${env.skyTop}, ${env.skyBottom} 62%, ${env.showGround ? env.groundColor : env.skyBottom} 62%)` }, onclick: () => ed.switchLevel(l.id), 'aria-label': `Open ${l.name}` },
        h('span', { class: 'asset-index' }, String(i + 1))),
      h('div', { class: 'asset-meta' },
        h('strong', {}, l.name),
        h('span', { class: 'muted small' }, `${l.objects.length} actors${isStart ? ' · start' : ''}${isOpen ? ' · open' : ''}`)),
      h('button', { type: 'button', class: 'icon-btn small asset-more', 'aria-label': `More for ${l.name}`, onclick: (e) => this.levelMenu(e, l, i) }, icon('menu')));
    }));
    this.body.append(bar, grid);
  }

  levelMenu(e, l, i) {
    const ed = this.editor;
    const doc = ed.doc;
    contextMenu(e.clientX, e.clientY, [
      { label: 'Open', icon: 'open', run: () => ed.switchLevel(l.id) },
      { label: 'Rename…', icon: 'file', run: async () => { const n = await askText('Rename level', { value: l.name, ok: 'Rename' }); if (n) ed.renameLevel(l.id, n); } },
      { label: 'Duplicate', icon: 'copy', run: () => ed.addLevel(null, { duplicateOf: l.id }) },
      { label: 'Play starts here', icon: 'play', disabled: l.id === doc.startLevel, run: () => ed.setStartLevel(l.id) },
      { label: 'Move earlier', icon: 'undo', disabled: i === 0, run: () => ed.moveLevel(l.id, -1) },
      { label: 'Move later', icon: 'redo', disabled: i === doc.levels.length - 1, run: () => ed.moveLevel(l.id, 1) },
      '-',
      { label: 'Delete level', icon: 'trash', danger: true, disabled: doc.levels.length <= 1, run: async () => {
        if (await confirmDialog('Delete level', `Delete "${l.name}" and everything in it? You can undo this.`, { ok: 'Delete', danger: true })) ed.deleteLevel(l.id);
      } },
    ]);
  }

  newLevel() {
    const grid = h('div', { class: 'template-grid' }, TEMPLATES.map((t) => h('button', {
      type: 'button', class: 'template-card', onclick: () => {
        d.close();
        const level = this.editor.addLevel(t.key === 'empty' ? null : t.label);
        if (t.key !== 'empty') {
          const view = applyActions(this.editor.project, templateActions(t.key).filter((a) => a.do !== 'settings')).project;
          this.editor.commit(view, { label: 'Fill level', source: 'levels' });
        }
        toast(`Added "${level.name}"`);
      },
    }, h('span', { class: `template-art art-${t.key}` }), h('strong', {}, t.key === 'empty' ? 'Empty level' : t.label), h('span', { class: 'muted small' }, t.blurb))));
    const d = openDialog({ title: 'New level', wide: true, body: h('div', {}, h('p', { class: 'muted' }, 'Start the new level empty or from a template. Game rules stay the same for every level.'), grid) });
  }

  renderPrefabs(doc) {
    const ed = this.editor;
    this.body.append(h('div', { class: 'cb-bar' },
      h('span', { class: 'muted small' }, 'Select actors, then right-click and choose "Save as prefab" to reuse them anywhere.')));
    if (!doc.prefabs.length) {
      this.body.append(h('p', { class: 'muted panel-pad' }, 'No prefabs yet.'));
      return;
    }
    this.body.append(h('div', { class: 'cb-grid' }, doc.prefabs.map((p) => h('div', {
      class: 'asset-card',
      oncontextmenu: (e) => { e.preventDefault(); this.prefabMenu(e, p); },
    },
    h('button', { type: 'button', class: 'asset-thumb prefab', onclick: () => ed.placePrefab(p.id), title: `Place ${p.name}` },
      h('span', { class: 'prefab-dots' }, p.objects.slice(0, 6).map((o) => h('span', { class: 'dot', style: { background: o.color } })))),
    h('div', { class: 'asset-meta' }, h('strong', {}, p.name), h('span', { class: 'muted small' }, `${p.objects.length} actor${p.objects.length === 1 ? '' : 's'}`)),
    h('button', { type: 'button', class: 'icon-btn small asset-more', 'aria-label': `More for ${p.name}`, onclick: (e) => this.prefabMenu(e, p) }, icon('menu'))))));
  }

  prefabMenu(e, p) {
    const ed = this.editor;
    contextMenu(e.clientX, e.clientY, [
      { label: 'Place in level', icon: 'plus', run: () => ed.placePrefab(p.id) },
      { label: 'Rename…', icon: 'file', run: async () => { const n = await askText('Rename prefab', { value: p.name, ok: 'Rename' }); if (n) ed.renamePrefab(p.id, n); } },
      '-',
      { label: 'Delete prefab', icon: 'trash', danger: true, run: () => ed.deletePrefab(p.id) },
    ]);
  }

  renderLibrary() {
    const grid = h('div', { class: 'cb-grid small' });
    renderThumbnails().then((thumbs) => {
      for (const g of PREFAB_GROUPS) {
        for (const item of g.items) {
          grid.append(h('button', { type: 'button', class: 'asset-card lib', title: `Place ${item.label}`, onclick: () => this.editor.addPrefab(item.key) },
            h('span', { class: 'asset-thumb' }, thumbs.get(item.key) ? h('img', { src: thumbs.get(item.key), alt: '' }) : null),
            h('span', { class: 'asset-meta' }, h('strong', {}, item.label), h('span', { class: 'muted small' }, g.label))));
        }
      }
    });
    this.body.append(grid);
  }

  renderScripts() {
    const p = this.editor.project;
    const rows = [h('button', { type: 'button', class: 'script-row', onclick: () => this.onOpenCode?.({ kind: 'game' }) },
      icon('file'), h('strong', {}, 'Game script'), h('span', { class: 'muted small' }, p.gameScript.trim() ? `${p.gameScript.split('\n').length} lines · runs in every level` : 'empty · runs in every level'))];
    for (const o of p.objects) {
      o.behaviors.forEach((b, index) => {
        if (b.type !== 'script') return;
        rows.push(h('button', { type: 'button', class: 'script-row', onclick: () => this.onOpenCode?.({ kind: 'object', id: o.id, index }) },
          icon('file'), h('strong', {}, o.name), h('span', { class: 'muted small' }, `${b.params.code.split('\n').length} lines · ${p.levelName}`)));
      });
    }
    this.body.append(h('div', { class: 'script-list' }, rows));
  }
}

// ---------------------------------------------------------------- Output Log
export class OutputLog {
  constructor(root) {
    this.root = root;
    this.filter = 'all';
    this.query = '';
    this.list = h('div', { class: 'log-list', role: 'log', 'aria-live': 'polite' });
    const filters = h('div', { class: 'seg' }, [['all', 'All'], ['info', 'Info'], ['warn', 'Warnings'], ['error', 'Errors']].map(([k, label]) => h('button', {
      type: 'button', class: `seg-btn ${k === this.filter ? 'on' : ''}`, dataset: { f: k }, onclick: () => { this.filter = k; for (const b of filters.children) b.classList.toggle('on', b.dataset.f === k); this.render(); },
    }, label)));
    const search = h('input', { type: 'search', class: 'input search', placeholder: 'Filter log', 'aria-label': 'Filter log' });
    search.addEventListener('input', () => { this.query = search.value.toLowerCase(); this.render(); });
    root.append(h('div', { class: 'log-bar' }, filters, search, h('button', { type: 'button', class: 'btn btn-small', onclick: () => clearLog() }, icon('trash'), 'Clear')), this.list);
    onLog((entry) => {
      if (!entry) { this.render(); return; }
      if (this.matches(entry)) this.appendEntry(entry, true);
      this.onEntry?.(entry);
    });
    this.render();
  }

  matches(e) {
    if (this.filter === 'info' && !(e.level === 'info' || e.level === 'success')) return false;
    if (this.filter === 'warn' && e.level !== 'warn') return false;
    if (this.filter === 'error' && e.level !== 'error') return false;
    return !this.query || e.text.toLowerCase().includes(this.query) || e.source.includes(this.query);
  }

  appendEntry(e, scroll) {
    const atBottom = this.list.scrollTop + this.list.clientHeight >= this.list.scrollHeight - 30;
    const t = new Date(e.time);
    this.list.append(h('div', { class: `log-line log-${e.level}` },
      h('span', { class: 'log-time' }, `${String(t.getHours()).padStart(2, '0')}:${String(t.getMinutes()).padStart(2, '0')}:${String(t.getSeconds()).padStart(2, '0')}`),
      h('span', { class: 'log-src' }, e.source),
      h('span', { class: 'log-text' }, e.text)));
    while (this.list.children.length > 1500) this.list.firstChild.remove();
    if (scroll && atBottom) this.list.scrollTop = this.list.scrollHeight;
  }

  render() {
    clear(this.list);
    for (const e of logEntries()) if (this.matches(e)) this.appendEntry(e, false);
    if (!this.list.children.length) this.list.append(h('p', { class: 'muted small panel-pad' }, 'Messages from your game, your scripts and the AI assistant show up here. Use log("hello") in a script.'));
    this.list.scrollTop = this.list.scrollHeight;
  }
}

// ---------------------------------------------------------------- Code editor
export const SCRIPT_REFERENCE = `// Actor scripts (a "Custom code" component) can define:
function onStart() {}          // once, when play starts
function onUpdate(dt) {}       // every frame (dt = seconds since last frame)
function onTouch() {}          // when the player touches this actor

// The game script (one per project) can define onStart() and onUpdate(dt).

// self     - this actor (not in the game script)
self.position.x += 1;          // position, rotation (radians) and scale are THREE objects
self.color = "#ff0000";
self.hide(); self.show(); self.destroy();
self.distanceToPlayer();        // meters
self.touchingPlayer;            // true or false
self.data.counter = 0;          // your own values

// game
game.time; game.score; game.lives; game.level; game.levelNumber;
game.addScore(10);
game.message("Hello!", 3);
game.sound("coin");            // coin, jump, hurt, bounce, win, lose, power, door, teleport, pop
game.win("You did it!"); game.lose("Too slow!");
game.completeLevel();          // go on to the next level
game.giveItem("key"); game.hasItem("key");

// player
player.position;               // a copy (THREE.Vector3)
player.grounded;
player.jump(12); player.hurt(1); player.teleport(0, 5, 0);

// world
world.find("Door");            // first actor with this name
world.findAll("enemy");        // actors with this name or tag
world.spawn({ shape: "sphere", position: [0, 5, 0], color: "orange", physics: "dynamic", lifetime: 5 });
world.count;

log("anything", 123);         // prints to the Output Log
// THREE (three.js) is available too.`;

export class CodePanel {
  constructor(root, editor, { onAskAI } = {}) {
    this.root = root;
    this.editor = editor;
    this.onAskAI = onAskAI;
    this.target = { kind: 'game' };
    this.pending = null;
    this.list = h('div', { class: 'code-list' });
    this.title = h('strong', {});
    this.status = h('span', { class: 'muted small code-status' });
    this.refBox = h('pre', { class: 'code-ref', hidden: true }, SCRIPT_REFERENCE);
    this.host = h('div', { class: 'code-host' });
    const bar = h('div', { class: 'code-bar' }, this.title, this.status,
      h('span', { class: 'spacer' }),
      h('button', { type: 'button', class: 'btn btn-small', onclick: () => this.check() }, icon('check'), 'Check'),
      h('button', { type: 'button', class: 'btn btn-small', onclick: () => this.apply(true) }, icon('save'), 'Apply', h('kbd', {}, 'Ctrl+S')),
      h('button', { type: 'button', class: 'btn btn-small', onclick: () => this.askAI() }, icon('sparkle'), 'Ask AI'),
      h('button', { type: 'button', class: 'btn btn-small', 'aria-pressed': 'false', onclick: (e) => { this.refBox.hidden = !this.refBox.hidden; e.currentTarget.setAttribute('aria-pressed', String(!this.refBox.hidden)); } }, icon('help'), 'Reference'));
    root.append(this.list, h('div', { class: 'code-main' }, bar, h('div', { class: 'code-split' }, this.host, this.refBox)));
    this.cm = createCodeEditor(this.host, {
      value: '',
      onChange: () => this.scheduleApply(),
      onSave: () => this.apply(true),
    });
    editor.on('change', ({ source }) => { if (source !== 'code') this.refresh(); });
    editor.on('level', () => { if (this.target.kind === 'object') this.target = { kind: 'game' }; this.refresh(); });
  }

  open(target) {
    this.flush();
    this.target = target;
    this.refresh(true);
    setTimeout(() => this.cm.focus(), 30);
  }

  currentCode() {
    const p = this.editor.project;
    if (!p) return null;
    if (this.target.kind === 'game') return p.gameScript;
    const o = p.objects.find((x) => x.id === this.target.id);
    const b = o?.behaviors[this.target.index];
    return b?.type === 'script' ? b.params.code : null;
  }

  refresh(force = false) {
    if (!this.editor.project) return;
    this.renderList();
    let code = this.currentCode();
    if (code === null) { this.target = { kind: 'game' }; code = this.currentCode(); this.renderList(); }
    const o = this.target.kind === 'object' ? this.editor.getEntity(this.target.id) : null;
    this.title.textContent = o ? `${o.name} · Custom code` : 'Game script';
    if (force || !this.pending) this.cm.setValue(code || '');
    this.cm.markError(null);
    this.status.textContent = '';
  }

  renderList() {
    const p = this.editor.project;
    clear(this.list);
    const item = (target, label, sub) => h('button', {
      type: 'button',
      class: `code-item ${sameTarget(target, this.target) ? 'on' : ''}`,
      onclick: () => this.open(target),
    }, h('strong', {}, label), h('span', { class: 'muted small' }, sub));
    this.list.append(h('p', { class: 'section-title panel-pad' }, 'Scripts'), item({ kind: 'game' }, 'Game script', 'All levels'));
    for (const o of p.objects) {
      o.behaviors.forEach((b, index) => { if (b.type === 'script') this.list.append(item({ kind: 'object', id: o.id, index }, o.name, p.levelName)); });
    }
    const sel = this.editor.selected;
    this.list.append(h('button', {
      type: 'button', class: 'btn btn-small add-script', disabled: !sel,
      title: sel ? `Add a script to ${sel.name}` : 'Select an actor first',
      onclick: () => {
        const e = this.editor.selected;
        if (!e) return;
        const index = e.behaviors.length;
        this.editor.updateEntity(e.id, { behaviors: [...e.behaviors, { type: 'script', params: { code: BEHAVIORS.script.params.code.default } }] }, 'inspector');
        this.open({ kind: 'object', id: e.id, index });
      },
    }, icon('plus'), 'New script'));
  }

  scheduleApply() {
    this.status.textContent = 'Editing...';
    clearTimeout(this.pending);
    this.pending = setTimeout(() => this.apply(false), 900);
  }

  flush() {
    if (this.pending) this.apply(false);
  }

  apply(manual) {
    clearTimeout(this.pending);
    this.pending = null;
    const code = this.cm.getValue();
    if (code === this.currentCode()) {
      if (manual) this.status.textContent = 'Up to date';
      return;
    }
    if (this.target.kind === 'game') this.editor.setGameScript(code, 'code');
    else {
      const o = this.editor.getEntity(this.target.id);
      if (!o) return;
      const behaviors = o.behaviors.map((b, i) => (i === this.target.index ? { ...b, params: { ...b.params, code } } : b));
      this.editor.updateEntity(o.id, { behaviors }, 'code');
    }
    this.status.textContent = 'Applied';
    if (manual) this.check(true);
  }

  check(quiet = false) {
    const code = this.cm.getValue();
    try {
      // Compiles without running, to find syntax mistakes.
      // eslint-disable-next-line no-new-func
      new Function('self', 'game', 'player', 'THREE', 'world', 'log', `"use strict";\n${code}`);
      this.cm.markError(null);
      if (!quiet) { this.status.textContent = 'No problems found'; logger.success('script', `${this.title.textContent}: no problems found.`); }
      return true;
    } catch (err) {
      const line = scriptErrorLine(err, 3);
      this.cm.markError(line);
      this.status.textContent = `${err.name}: ${err.message}`;
      logger.error('script', `${this.title.textContent}: ${err.message}`);
      return false;
    }
  }

  /** Called when the running game reports an error. */
  showRuntimeError({ id, line, message }) {
    const isGame = id === null;
    if ((isGame && this.target.kind !== 'game') || (!isGame && this.target.id !== id)) return;
    this.cm.markError(line);
    this.status.textContent = `Error${line ? ` on line ${line}` : ''}: ${message}`;
  }

  askAI() {
    const o = this.target.kind === 'object' ? this.editor.getEntity(this.target.id) : null;
    this.onAskAI?.(o ? `Write a script for "${o.name}" that ` : 'Write a game script that ');
  }
}

function sameTarget(a, b) {
  return a.kind === b.kind && (a.kind === 'game' || (a.id === b.id && a.index === b.index));
}

// ---------------------------------------------------------------- viewport toolbar (Pro)
export class ViewportBar {
  constructor(root, editor, { onToggleStats } = {}) {
    this.root = root;
    this.editor = editor;
    this.onToggleStats = onToggleStats;
    this.showStats = false;
    editor.on('tool', () => this.render());
    editor.on('select', () => this.render());
    this.render();
  }

  render() {
    const ed = this.editor;
    clear(this.root);
    const tool = (mode, ic, label, key) => h('button', { type: 'button', class: 'vb-btn', title: `${label} (${key})`, 'aria-label': label, 'aria-pressed': String(ed.gizmoMode === mode), onclick: () => ed.setGizmoMode(mode) }, icon(ic));
    const select = (label, value, options, onChange) => {
      const s = h('select', { class: 'vb-select', 'aria-label': label, title: label }, options.map(([v, l]) => h('option', { value: String(v), selected: String(v) === String(value) }, l)));
      s.addEventListener('change', () => onChange(s.value));
      return s;
    };
    this.root.append(
      h('div', { class: 'vb-group' },
        tool('translate', 'move', 'Move', 'W'), tool('rotate', 'rotate', 'Rotate', 'E'), tool('scale', 'scale', 'Scale', 'R'),
        h('button', { type: 'button', class: 'vb-btn wide', title: 'Move and rotate along the world axes or the actor\'s own axes', onclick: () => ed.setSpace(ed.space === 'world' ? 'local' : 'world') }, icon('globe'), ed.space === 'world' ? 'World' : 'Local')),
      h('div', { class: 'vb-group' },
        h('button', { type: 'button', class: 'vb-btn', title: 'Snap', 'aria-label': 'Snap', 'aria-pressed': String(ed.snap), onclick: () => ed.setSnap(!ed.snap) }, icon('magnet')),
        select('Move snap', ed.snapValues.move, [[0.1, '0.1 m'], [0.25, '0.25 m'], [0.5, '0.5 m'], [1, '1 m'], [5, '5 m']], (v) => ed.setSnapValue('move', parseFloat(v))),
        select('Rotate snap', ed.snapValues.rotate, [[5, '5°'], [15, '15°'], [45, '45°'], [90, '90°']], (v) => ed.setSnapValue('rotate', parseFloat(v))),
        select('Scale snap', ed.snapValues.scale, [[0.05, '×0.05'], [0.1, '×0.1'], [0.25, '×0.25'], [0.5, '×0.5']], (v) => ed.setSnapValue('scale', parseFloat(v)))),
      h('span', { class: 'spacer' }),
      h('div', { class: 'vb-group' },
        h('span', { class: 'vb-label', title: 'Camera speed while flying (hold right mouse, use W A S D Q E, scroll to change)' }, icon('focus'), `${Math.round(ed.flySpeed)} m/s`),
        select('View mode', ed.viewMode, [['lit', 'Lit'], ['unlit', 'Unlit'], ['wireframe', 'Wireframe']], (v) => ed.setViewMode(v)),
        h('button', { type: 'button', class: 'vb-btn', title: 'Grid', 'aria-label': 'Grid', 'aria-pressed': String(ed.grid.visible), onclick: () => ed.setGridVisible(!ed.grid.visible) }, icon('list')),
        h('button', { type: 'button', class: 'vb-btn wide', title: 'Show performance stats', 'aria-pressed': String(this.showStats), onclick: () => { this.showStats = !this.showStats; this.onToggleStats?.(this.showStats); this.render(); } }, 'Stats')));
  }
}

export class StatsOverlay {
  constructor(root) {
    this.root = root;
    this.frames = 0;
    this.elapsed = 0;
    this.root.hidden = true;
  }

  update(dt, renderer, objects) {
    if (this.root.hidden) return;
    this.frames++;
    this.elapsed += dt;
    if (this.elapsed < 0.5) return;
    const fps = Math.round(this.frames / this.elapsed);
    this.frames = 0;
    this.elapsed = 0;
    const r = renderer.info.render;
    this.root.textContent = `${fps} FPS · ${objects} actors · ${r.calls} draw calls · ${(r.triangles / 1000).toFixed(1)}k triangles`;
  }
}
