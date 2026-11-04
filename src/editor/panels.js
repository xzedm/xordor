// The editor's side panels: Add (the palette), Objects (the list) and
// Properties (settings for the selected object, or the whole world).
import { clear, h, icon } from '../ui/dom.js';
import { PREFAB_GROUPS } from '../core/prefabs.js';
import { SHAPES } from '../core/shapes.js';
import { BEHAVIORS, BEHAVIOR_KEYS, defaultParams } from '../core/behaviors.js';
import { CAMERAS, LOOKS, PATTERNS, PHYSICS, WIN_CONDITIONS, halfHeight } from '../core/project.js';
import { ENV_PRESETS } from '../core/environment.js';
import { renderThumbnails } from './thumbnails.js';
import { checkField, colorField, numberField, rangeField, section, selectField, textField, vec3Field } from './fields.js';

// ---------------------------------------------------------------- Add panel
export class AddPanel {
  constructor(root, editor) {
    this.root = root;
    this.editor = editor;
    this.query = '';
    this.render();
    renderThumbnails().then((thumbs) => {
      for (const img of this.root.querySelectorAll('img[data-thumb]')) {
        const src = thumbs.get(img.dataset.thumb);
        if (src) { img.src = src; img.closest('.tile')?.classList.add('has-thumb'); }
      }
    });
  }

  render() {
    clear(this.root);
    const search = h('input', { type: 'search', class: 'input search', placeholder: 'Search things to add', 'aria-label': 'Search things to add', value: this.query });
    search.addEventListener('input', () => {
      this.query = search.value.trim().toLowerCase();
      this.filter();
    });
    this.root.append(h('div', { class: 'panel-pad' }, search));
    for (const group of PREFAB_GROUPS) {
      const grid = h('div', { class: 'tile-grid' }, group.items.map((item) => h('button', {
        type: 'button',
        class: 'tile',
        title: `Add ${item.label}`,
        dataset: { label: `${item.label} ${item.key} ${item.shape} ${group.label}`.toLowerCase() },
        onclick: () => this.editor.addPrefab(item.key),
      }, h('img', { alt: '', dataset: { thumb: item.key }, width: 56, height: 56 }), h('span', {}, item.label))));
      this.root.append(h('section', { class: 'panel-section tile-group' }, h('h3', { class: 'section-title' }, group.label), grid));
    }
  }

  filter() {
    for (const tile of this.root.querySelectorAll('.tile')) tile.hidden = !!this.query && !tile.dataset.label.includes(this.query);
    for (const g of this.root.querySelectorAll('.tile-group')) g.hidden = ![...g.querySelectorAll('.tile')].some((t) => !t.hidden);
  }
}

// ---------------------------------------------------------------- Objects panel
export class ObjectsPanel {
  constructor(root, editor) {
    this.root = root;
    this.editor = editor;
    this.query = '';
    const search = h('input', { type: 'search', class: 'input search', placeholder: 'Find an object', 'aria-label': 'Find an object' });
    search.addEventListener('input', () => { this.query = search.value.trim().toLowerCase(); this.render(); });
    this.list = h('ul', { class: 'object-list', role: 'listbox', 'aria-label': 'Objects in your world' });
    this.count = h('p', { class: 'muted small' });
    root.append(h('div', { class: 'panel-pad' }, search, this.count), this.list);
    editor.on('change', () => this.render());
    editor.on('select', () => this.render());
  }

  render() {
    const p = this.editor.project;
    if (!p) return;
    const scroll = this.list.scrollTop;
    clear(this.list);
    const items = p.objects.filter((o) => !this.query || o.name.toLowerCase().includes(this.query) || o.shape.includes(this.query));
    this.count.textContent = `${p.objects.length} object${p.objects.length === 1 ? '' : 's'} in your world`;
    const shown = items.slice(0, 600);
    for (const o of shown) {
      const selected = o.id === this.editor.selectedId;
      const li = h('li', { class: `object-row ${selected ? 'selected' : ''}`, role: 'option', 'aria-selected': selected ? 'true' : 'false' },
        h('span', { class: 'dot', style: { background: o.color } }),
        h('button', {
          type: 'button', class: 'object-name',
          onclick: () => this.editor.select(o.id),
          ondblclick: () => { this.editor.select(o.id); this.editor.focusSelected(); },
        }, o.name, h('span', { class: 'muted' }, ` ${SHAPES[o.shape]?.label || o.shape}`)),
        h('button', {
          type: 'button', class: 'icon-btn small', title: o.visible ? 'Hide while playing' : 'Show while playing', 'aria-label': o.visible ? `Hide ${o.name}` : `Show ${o.name}`,
          onclick: () => this.editor.updateEntity(o.id, { visible: !o.visible }, 'objects'),
        }, icon(o.visible ? 'eye' : 'eyeOff')));
      this.list.append(li);
    }
    if (items.length > shown.length) this.list.append(h('li', { class: 'muted small object-more' }, `${items.length - shown.length} more. Use the search box to find them.`));
    if (!items.length) this.list.append(h('li', { class: 'muted small object-more' }, this.query ? 'Nothing matches that search.' : 'Your world is empty. Add something from the Add tab.'));
    this.list.scrollTop = scroll;
  }
}

// ---------------------------------------------------------------- Properties
export class Inspector {
  constructor(root, editor, { onAskAI } = {}) {
    this.root = root;
    this.editor = editor;
    this.onAskAI = onAskAI;
    this.transformFields = {};
    editor.on('select', () => this.render());
    editor.on('change', ({ source }) => {
      if (source === 'inspector-field') return;
      this.render();
    });
    editor.on('transform', (t) => {
      if (t.id !== editor.selectedId) return;
      this.transformFields.position?.setValues(t.position);
      this.transformFields.rotation?.setValues(t.rotation);
      this.transformFields.scale?.setValues(t.scale);
    });
  }

  render() {
    const scroll = this.root.scrollTop;
    clear(this.root);
    this.transformFields = {};
    const e = this.editor.selected;
    if (e) this.renderEntity(e);
    else if (this.editor.project) this.renderWorld(this.editor.project);
    this.root.scrollTop = scroll;
  }

  set(patch, structural = false) {
    this.editor.updateEntity(this.editor.selectedId, patch, structural ? 'inspector' : 'inspector-field');
  }

  renderEntity(e) {
    const ed = this.editor;
    const nameInput = h('input', { type: 'text', class: 'input title-input', value: e.name, 'aria-label': 'Object name' });
    nameInput.addEventListener('change', () => this.set({ name: nameInput.value }, true));
    const head = h('div', { class: 'inspector-head' },
      nameInput,
      h('div', { class: 'head-actions' },
        h('span', { class: 'chip' }, SHAPES[e.shape]?.label || e.shape),
        h('button', { type: 'button', class: 'btn btn-small', onclick: () => this.onAskAI?.(e) }, icon('sparkle'), 'Ask AI'),
        h('button', { type: 'button', class: 'icon-btn', title: 'Duplicate (Ctrl+D)', 'aria-label': 'Duplicate', onclick: () => ed.duplicateSelected() }, icon('copy')),
        h('button', { type: 'button', class: 'icon-btn danger', title: 'Delete (Del)', 'aria-label': 'Delete', onclick: () => ed.deleteSelected() }, icon('trash'))));

    const pos = vec3Field('Position', e.position, (v) => this.set({ position: v }), { step: 0.25, key: 'position' });
    const rot = vec3Field('Turn (degrees)', e.rotation, (v) => this.set({ rotation: v }), { step: 15, key: 'rotation' });
    const scl = vec3Field('Size (times)', e.scale, (v) => this.set({ scale: v }), { step: 0.1, min: 0.02, key: 'scale' });
    this.transformFields = { position: pos, rotation: rot, scale: scl };
    const def = SHAPES[e.shape];
    const sizeNote = h('p', { class: 'field-hint' }, `Actual size: ${e.scale.map((s, i) => (s * def.size[i]).toFixed(2)).join(' x ')} m`);
    const transform = section('Place and size', pos, rot, scl, sizeNote,
      h('div', { class: 'btn-row' },
        h('button', { type: 'button', class: 'btn btn-small', onclick: () => this.set({ position: [e.position[0], halfHeight(e), e.position[2]] }, true) }, icon('ground'), 'Put on ground'),
        h('button', { type: 'button', class: 'btn btn-small', onclick: () => this.set({ rotation: [0, 0, 0], scale: [1, 1, 1] }, true) }, 'Reset turn and size')));

    const look = section('Look',
      colorField('Color', e.color, (c) => this.set({ color: c }, true)),
      selectField('Material', e.look, LOOKS, (v) => this.set({ look: v }, true)),
      selectField('Pattern', e.pattern, PATTERNS, (v) => this.set({ pattern: v }, true)),
      checkField('Visible while playing', e.visible, (v) => this.set({ visible: v }, true)));

    const extras = def.props ? section('Extras', ...Object.entries(def.props).map(([k, p]) => (p.type === 'number'
      ? numberField(p.label, e.props[k], (v) => this.set({ props: { ...e.props, [k]: v } }, true), p)
      : textField(p.label, e.props[k], (v) => this.set({ props: { ...e.props, [k]: v } }, true))))) : null;

    const physicsFields = [selectField('How solid is it?', e.physics, PHYSICS, (v) => this.set({ physics: v }, true))];
    if (e.physics === 'dynamic') {
      physicsFields.push(numberField('Weight (kg)', e.mass, (v) => this.set({ mass: v }), { min: 0.01, step: 0.5 }));
    }
    if (e.physics !== 'none') physicsFields.push(rangeField('Bounciness', e.bounciness, (v) => this.set({ bounciness: v }), { format: (v) => `${Math.round(v * 100)}%` }));
    const physics = section('Physics', ...physicsFields);

    const behaviorList = h('div', { class: 'behavior-list' }, e.behaviors.map((b, i) => this.behaviorCard(e, b, i)));
    const picker = h('select', { class: 'input', 'aria-label': 'Add a behavior' },
      h('option', { value: '' }, 'Add a behavior...'),
      ...['Movement', 'Game rules', 'Advanced'].map((g) => h('optgroup', { label: g },
        BEHAVIOR_KEYS.filter((k) => BEHAVIORS[k].group === g).map((k) => h('option', { value: k }, `${BEHAVIORS[k].label}: ${BEHAVIORS[k].desc}`)))));
    picker.addEventListener('change', () => {
      const type = picker.value;
      if (!type) return;
      const list = e.behaviors.filter((b) => b.type !== type || type === 'script');
      this.set({ behaviors: [...list, { type, params: defaultParams(type) }] }, true);
    });
    const behaviors = section('Behaviors', e.behaviors.length ? null : h('p', { class: 'muted small' }, 'Behaviors make things move or follow game rules. Pick one below.'), behaviorList, picker);

    this.root.append(...[head, transform, look, extras, physics, behaviors].filter(Boolean));
  }

  behaviorCard(e, b, index) {
    const def = BEHAVIORS[b.type];
    const update = (k, v, structural = false) => {
      const list = e.behaviors.map((x, i) => (i === index ? { ...x, params: { ...x.params, [k]: v } } : x));
      this.set({ behaviors: list }, structural);
    };
    const remove = () => this.set({ behaviors: e.behaviors.filter((_, i) => i !== index) }, true);
    const fields = Object.entries(def.params).map(([k, p]) => {
      const v = b.params[k];
      switch (p.type) {
        case 'number': return numberField(p.label, v, (nv) => update(k, nv), p);
        case 'select': return selectField(p.label, v, Object.fromEntries(p.options.map((o) => [o, o.toUpperCase()])), (nv) => update(k, nv));
        case 'boolean': return checkField(p.label, v, (nv) => update(k, nv));
        case 'color': return colorField(p.label, v, (nv) => update(k, nv, true), { swatches: false });
        case 'vec3': return vec3Field(p.label, v, (nv) => update(k, nv), { step: 0.5 });
        case 'code': return textField(p.label, v, (nv) => update(k, nv), { multiline: true, code: true, hint: 'Runs while playing. Define onStart(), onUpdate(dt) or onTouch(). Ask the AI helper to write it for you.' });
        default: return textField(p.label, v ?? '', (nv) => update(k, nv));
      }
    });
    return h('div', { class: 'behavior-card' },
      h('div', { class: 'behavior-head' },
        h('div', {}, h('strong', {}, def.label), h('p', { class: 'muted small' }, def.desc)),
        h('button', { type: 'button', class: 'icon-btn small', 'aria-label': `Remove ${def.label}`, title: 'Remove', onclick: remove }, icon('close'))),
      ...fields);
  }

  renderWorld(p) {
    const ed = this.editor;
    const env = p.environment;
    const s = p.settings;
    const setEnv = (patch) => ed.setEnvironment(patch, 'inspector');
    const setRule = (patch, structural = false) => ed.setSettings(patch, structural ? 'inspector' : 'inspector-field');

    const head = h('div', { class: 'inspector-head' },
      h('div', {}, h('h2', { class: 'world-title' }, 'World and rules'), h('p', { class: 'muted small' }, 'Nothing is selected, so these settings are for the whole game. Click an object to edit it.')));

    const presets = h('div', { class: 'preset-grid' }, Object.entries(ENV_PRESETS).map(([k, v]) => h('button', {
      type: 'button',
      class: `preset ${env.preset === k ? 'on' : ''}`,
      style: { background: `linear-gradient(180deg, ${v.skyTop}, ${v.skyBottom} 70%, ${v.groundColor} 70%)` },
      onclick: () => setEnv({ ...envPreset(k) }),
    }, h('span', {}, v.label))));

    const sky = section('Sky and light', presets,
      colorField('Sky color (top)', env.skyTop, (c) => setEnv({ skyTop: c }), { swatches: false }),
      colorField('Sky color (horizon)', env.skyBottom, (c) => setEnv({ skyBottom: c }), { swatches: false }),
      colorField('Sunlight color', env.sunColor, (c) => setEnv({ sunColor: c }), { swatches: false }),
      rangeField('Sun brightness', env.sunIntensity, (v) => setEnv({ sunIntensity: v }), { min: 0, max: 6, step: 0.1, format: (v) => v.toFixed(1) }),
      rangeField('Sun height', env.sunHeight, (v) => setEnv({ sunHeight: v }), { min: 2, max: 90, step: 1, format: (v) => `${Math.round(v)}°` }),
      rangeField('Fill light', env.ambient, (v) => setEnv({ ambient: v }), { min: 0, max: 3, step: 0.05, format: (v) => v.toFixed(2) }),
      rangeField('Fog', env.fog, (v) => setEnv({ fog: v }), { min: 0, max: 1, step: 0.01, format: (v) => `${Math.round(v * 100)}%` }),
      colorField('Fog color', env.fogColor, (c) => setEnv({ fogColor: c }), { swatches: false }),
      checkField('Show stars', env.stars, (v) => setEnv({ stars: v })));

    const ground = section('Ground',
      checkField('Show the ground', env.showGround, (v) => setEnv({ showGround: v }), { hint: 'Turn this off for sky levels where players can fall.' }),
      colorField('Ground color', env.groundColor, (c) => setEnv({ groundColor: c })),
      selectField('Ground pattern', env.groundPattern, PATTERNS, (v) => setEnv({ groundPattern: v })),
      numberField('Ground size (meters)', env.groundSize, (v) => setEnv({ groundSize: v }), { min: 4, max: 2000, step: 10 }));

    const rules = section('Game rules',
      selectField('Camera', s.camera, CAMERAS, (v) => setRule({ camera: v }, true)),
      selectField('How to win', s.winCondition, WIN_CONDITIONS, (v) => setRule({ winCondition: v }, true)),
      s.winCondition === 'score' ? numberField('Points needed', s.targetScore, (v) => setRule({ targetScore: v }), { min: 1, step: 1 }) : null,
      numberField('Lives (0 = unlimited)', s.lives, (v) => setRule({ lives: v }), { min: 0, max: 99, step: 1 }),
      numberField('Time limit in seconds (0 = none)', s.timeLimit, (v) => setRule({ timeLimit: v }), { min: 0, max: 3600, step: 5 }),
      textField('Message before playing', s.startMessage, (v) => setRule({ startMessage: v }), { placeholder: 'Leave empty to explain how to win' }));

    const player = section('Player',
      colorField('Player color', s.playerColor, (c) => setRule({ playerColor: c }, true)),
      rangeField('Running speed', s.playerSpeed, (v) => setRule({ playerSpeed: v }), { min: 1, max: 30, step: 0.5, format: (v) => v.toFixed(1) }),
      rangeField('Jump power', s.jumpPower, (v) => setRule({ jumpPower: v }), { min: 0, max: 30, step: 0.5, format: (v) => v.toFixed(1) }),
      rangeField('Gravity', s.gravity, (v) => setRule({ gravity: v }), { min: 1, max: 60, step: 1, format: (v) => (v < 12 ? `${v} (floaty)` : v > 35 ? `${v} (heavy)` : `${v}`) }),
      numberField('Fall out of the world below height', s.fallLimit, (v) => setRule({ fallLimit: v }), { min: -1000, max: 1000, step: 1 }));

    this.root.append(...[head, sky, ground, rules, player].filter(Boolean));
  }
}

function envPreset(key) {
  const { label, ...values } = ENV_PRESETS[key];
  return { preset: key, ...values };
}
