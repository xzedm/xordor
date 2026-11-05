// The editor: owns the project, undo history, selection and the 3D view you
// build in. Panels talk to it through small methods and "change" events.
//
// A project has several levels. `editor.project` is the *view* of the level
// being edited (game-wide settings plus that level's sky and objects), which is
// what panels, the AI helper and the game work with. `editor.doc` is the whole
// project that gets saved.
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { TransformControls } from 'three/addons/controls/TransformControls.js';
import { WorldView } from '../engine/world.js';
import {
  createEntity, createLevel, halfHeight, levelView, mergeLevelView, normalizeEntity, normalizeEnvironment,
  normalizeLevel, normalizePrefab, normalizeProject, normalizeSettings, normalizeView,
} from '../core/project.js';
import { prefabToEntity } from '../core/prefabs.js';
import { DEG, deepClone, newId, round } from '../core/util.js';
import { logger } from '../core/log.js';
import { ensureModel, forgetModel } from '../engine/models.js';

const MAX_HISTORY = 120;
const _m = new THREE.Matrix4();

export class Editor {
  constructor({ canvas, container }) {
    this.canvas = canvas;
    this.container = container;
    this.listeners = new Map();
    this.doc = null;
    this.assets = new Map(); // imported models, kept out of the undo history (they can be big)
    this.levelId = null;
    this._view = null;
    this.selection = [];
    this.undoStack = [];
    this.redoStack = [];
    this.gizmoMode = 'translate';
    this.snap = true;
    this.snapValues = { move: 0.25, rotate: 15, scale: 0.1 };
    this.space = 'world';
    this.navigation = 'orbit'; // 'orbit' (Simple) or 'unreal' (Pro: hold right mouse to fly)
    this.viewMode = 'lit';
    this.flySpeed = 10;
    this.active = true;
    this.dirty = false;

    this.view = new WorldView({ mode: 'edit' });
    this.view.onRebuild = (id) => {
      if (this.selection.includes(id)) this.refreshSelection();
      this.emit('rebuild', id);
    };
    this.scene = this.view.scene;
    this.camera = new THREE.PerspectiveCamera(55, 1, 0.1, 3000);
    this.camera.position.set(14, 11, 16);

    this.grid = new THREE.GridHelper(80, 80, 0x000000, 0x000000);
    this.grid.material.transparent = true;
    this.grid.material.opacity = 0.08;
    this.grid.material.depthWrite = false;
    this.grid.position.y = 0.01;
    this.scene.add(this.grid);

    this.orbit = new OrbitControls(this.camera, canvas);
    this.orbit.enableDamping = true;
    this.orbit.dampingFactor = 0.12;
    this.orbit.maxPolarAngle = Math.PI * 0.495;
    this.orbit.minDistance = 1;
    this.orbit.maxDistance = 1500;
    this.orbit.screenSpacePanning = false;
    this.orbit.target.set(0, 1, -2);
    this.orbit.update();

    this.pivot = new THREE.Object3D();
    this.scene.add(this.pivot);
    this.gizmo = new TransformControls(this.camera, canvas);
    this.gizmo.setSize(0.9);
    this.scene.add(this.gizmo.getHelper());
    this.gizmo.addEventListener('dragging-changed', (e) => {
      this.orbit.enabled = !e.value && !this.flying;
      this.dragging = e.value;
      if (e.value) this.beginGroupDrag();
    });
    this.gizmo.addEventListener('objectChange', () => this.onGizmoMove());
    this.gizmo.addEventListener('mouseUp', () => this.onGizmoRelease());
    this.applySnap();

    this.boxes = [];
    this.raycaster = new THREE.Raycaster();
    this.pointer = new THREE.Vector2();
    this.wireMaterial = new THREE.MeshBasicMaterial({ color: 0x8fd3ff, wireframe: true });
    this.bindPointer();
    this.bindFly();
  }

  // ---------- events ----------
  on(type, fn) {
    if (!this.listeners.has(type)) this.listeners.set(type, new Set());
    this.listeners.get(type).add(fn);
    return () => this.listeners.get(type)?.delete(fn);
  }

  emit(type, detail) {
    for (const fn of [...(this.listeners.get(type) || [])]) fn(detail);
  }

  // ---------- project, levels and history ----------
  /** The level being edited, as a view. */
  get project() {
    if (!this.doc) return null;
    if (!this._view) {
      this._view = levelView(this.doc, this.levelId);
      this._view.models = [...this.assets.values()].map((a) => ({ id: a.id, name: a.name, animations: a.animations }));
    }
    return this._view;
  }

  /** The whole project including imported models, for saving and exporting. */
  fullDoc() {
    return { ...this.doc, assets: [...this.assets.values()] };
  }

  addAsset(asset) {
    this.assets.set(asset.id, asset);
    this._view = null;
    ensureModel(asset).catch((err) => logger.error('editor', `Could not load the model "${asset.name}": ${err.message}`));
    this.emit('assets');
  }

  assetUsage(id) {
    let n = 0;
    for (const list of [...this.doc.levels.map((l) => l.objects), ...this.doc.prefabs.map((p) => p.objects)]) {
      for (const o of list) if (o.shape === 'model' && o.props?.asset === id) n++;
    }
    return n;
  }

  deleteAsset(id) {
    if (this.assetUsage(id)) return false;
    this.assets.delete(id);
    forgetModel(id);
    this._view = null;
    this.emit('assets');
    return true;
  }

  renameAsset(id, name) {
    const a = this.assets.get(id);
    if (!a || !name) return;
    a.name = String(name).trim().slice(0, 60) || a.name;
    this._view = null;
    this.dirty = true;
    this.emit('assets');
  }

  /** Puts a copy of an imported model in front of the camera, resting on the floor. */
  placeModel(assetId) {
    const asset = this.assets.get(assetId);
    if (!asset) return null;
    const [x, , z] = this.dropPoint();
    const entity = createEntity('model', { name: asset.name, position: [x, 1, z], scale: [2, 2, 2], physics: 'static', props: { asset: assetId } });
    this.mutate((d) => { d.objects.push(entity); }, { label: `Add ${asset.name}` });
    this.select(entity.id);
    ensureModel(asset).then(() => {
      requestAnimationFrame(() => { if (this.getEntity(entity.id)) this.dropSelectedToFloor(); });
    }).catch(() => {});
    return entity;
  }

  get level() {
    return this.doc?.levels.find((l) => l.id === this.levelId) || null;
  }

  load(project, { levelId = null } = {}) {
    const p = normalizeProject(project);
    this.assets = new Map();
    for (const a of p.assets) this.addAsset(a);
    p.assets = [];
    this.doc = p;
    this.levelId = this.doc.levels.some((l) => l.id === levelId) ? levelId : this.doc.startLevel;
    this._view = null;
    this.undoStack = [];
    this.redoStack = [];
    this.dirty = false;
    this.selection = [];
    this.view.sync(this.project);
    this.refreshSelection();
    this.emit('select', null);
    this.emit('level', this.levelId);
    this.emit('change', { source: 'load' });
  }

  snapshot() {
    return { json: JSON.stringify(this.doc), levelId: this.levelId };
  }

  pushUndo(label) {
    this.undoStack.push({ ...this.snapshot(), label });
    if (this.undoStack.length > MAX_HISTORY) this.undoStack.shift();
    this.redoStack = [];
  }

  /** Replaces the whole project (levels, prefabs...) as one undo step. */
  commitDoc(nextDoc, { source = 'edit', label = '' } = {}) {
    const normalized = normalizeProject(nextDoc);
    if (JSON.stringify(normalized) === JSON.stringify(this.doc)) return false;
    this.pushUndo(label);
    this.doc = normalized;
    if (!this.doc.levels.some((l) => l.id === this.levelId)) this.levelId = this.doc.startLevel;
    this.afterDocChange(source, label);
    return true;
  }

  /** Replaces the current level view with a modified copy, as one undo step. */
  commit(nextView, { source = 'edit', label = '' } = {}) {
    const next = normalizeView({ ...nextView, levelId: this.levelId, levels: this.project.levels });
    const before = normalizeView(this.project);
    if (JSON.stringify(next) === JSON.stringify(before)) return false;
    this.pushUndo(label);
    this.doc = mergeLevelView(this.doc, next);
    this.afterDocChange(source, label);
    return true;
  }

  afterDocChange(source, label) {
    this._view = null;
    this.dirty = true;
    this.view.sync(this.project);
    this.selection = this.selection.filter((id) => this.getEntity(id));
    this.refreshSelection();
    this.emit('change', { source, label });
  }

  /** Changes the current level with a function that edits a draft copy. */
  mutate(fn, opts) {
    const draft = deepClone(this.project);
    fn(draft);
    return this.commit(draft, opts);
  }

  mutateDoc(fn, opts) {
    const draft = deepClone(this.doc);
    fn(draft);
    return this.commitDoc(draft, opts);
  }

  get historyDepth() {
    return this.undoStack.length;
  }

  restore(step) {
    this.doc = JSON.parse(step.json);
    const levelChanged = step.levelId !== this.levelId && this.doc.levels.some((l) => l.id === step.levelId);
    if (levelChanged) this.levelId = step.levelId;
    if (!this.doc.levels.some((l) => l.id === this.levelId)) this.levelId = this.doc.startLevel;
    this._view = null;
    this.dirty = true;
    this.view.sync(this.project);
    this.selection = this.selection.filter((id) => this.getEntity(id));
    this.refreshSelection();
    if (levelChanged) this.emit('level', this.levelId);
  }

  undo() {
    const step = this.undoStack.pop();
    if (!step) return false;
    this.redoStack.push({ ...this.snapshot(), label: step.label });
    this.restore(step);
    this.emit('change', { source: 'undo' });
    return true;
  }

  redo() {
    const step = this.redoStack.pop();
    if (!step) return false;
    this.undoStack.push({ ...this.snapshot(), label: step.label });
    this.restore(step);
    this.emit('change', { source: 'redo' });
    return true;
  }

  markSaved() {
    this.dirty = false;
    this.emit('saved');
  }

  switchLevel(id) {
    if (!this.doc.levels.some((l) => l.id === id) || id === this.levelId) return;
    this.levelId = id;
    this._view = null;
    this.selection = [];
    this.view.sync(this.project);
    this.refreshSelection();
    this.emit('select', null);
    this.emit('level', id);
    this.emit('change', { source: 'level' });
    this.focusSelected();
  }

  addLevel(name, { duplicateOf = null } = {}) {
    let level;
    if (duplicateOf) {
      const src = this.doc.levels.find((l) => l.id === duplicateOf);
      level = normalizeLevel({ ...deepClone(src), id: newId('level'), name: name || `${src.name} copy`, objects: src.objects.map((o) => ({ ...deepClone(o), id: newId() })) });
    } else {
      level = createLevel(name || `Level ${this.doc.levels.length + 1}`);
    }
    this.mutateDoc((d) => { d.levels.push(level); }, { label: 'Add level', source: 'levels' });
    this.switchLevel(level.id);
    return level;
  }

  renameLevel(id, name) {
    this.mutateDoc((d) => { const l = d.levels.find((x) => x.id === id); if (l) l.name = String(name || '').trim() || l.name; }, { label: 'Rename level', source: 'levels' });
  }

  deleteLevel(id) {
    if (this.doc.levels.length <= 1) return false;
    const idx = this.doc.levels.findIndex((l) => l.id === id);
    this.mutateDoc((d) => {
      d.levels = d.levels.filter((l) => l.id !== id);
      if (d.startLevel === id) d.startLevel = d.levels[0].id;
    }, { label: 'Delete level', source: 'levels' });
    if (this.levelId === id || !this.level) this.switchLevel(this.doc.levels[Math.max(0, idx - 1)].id);
    return true;
  }

  moveLevel(id, delta) {
    this.mutateDoc((d) => {
      const i = d.levels.findIndex((l) => l.id === id);
      const j = i + delta;
      if (i < 0 || j < 0 || j >= d.levels.length) return;
      [d.levels[i], d.levels[j]] = [d.levels[j], d.levels[i]];
    }, { label: 'Reorder levels', source: 'levels' });
  }

  setStartLevel(id) {
    this.mutateDoc((d) => { d.startLevel = id; }, { label: 'Set start level', source: 'levels' });
  }

  setGameScript(code, source = 'code') {
    this.mutate((d) => { d.gameScript = code; }, { source, label: 'Edit game script' });
  }

  // ---------- prefabs made by the user ----------
  saveSelectionAsPrefab(name) {
    const ents = this.selectedEntities.filter((e) => e.shape !== 'spawn');
    if (!ents.length) return null;
    const cx = ents.reduce((s, e) => s + e.position[0], 0) / ents.length;
    const cz = ents.reduce((s, e) => s + e.position[2], 0) / ents.length;
    const bottom = Math.min(...ents.map((e) => e.position[1] - halfHeight(e)));
    const prefab = normalizePrefab({
      name: name || (ents.length === 1 ? ents[0].name : `${ents[0].name} group`),
      objects: ents.map((e) => ({ ...deepClone(e), position: [e.position[0] - cx, e.position[1] - bottom, e.position[2] - cz] })),
    });
    this.mutateDoc((d) => { d.prefabs.push(prefab); }, { label: 'Save prefab', source: 'prefabs' });
    logger.info('editor', `Saved prefab "${prefab.name}" with ${prefab.objects.length} object(s).`);
    return prefab;
  }

  placePrefab(prefabId) {
    const prefab = this.doc.prefabs.find((p) => p.id === prefabId);
    if (!prefab) return;
    const [x, , z] = this.dropPoint();
    const placed = prefab.objects.map((o) => normalizeEntity({ ...deepClone(o), id: newId(), position: [o.position[0] + x, o.position[1], o.position[2] + z] }));
    this.mutate((d) => { d.objects.push(...placed); }, { label: `Place ${prefab.name}` });
    this.selectMany(placed.map((e) => e.id));
  }

  deletePrefab(prefabId) {
    this.mutateDoc((d) => { d.prefabs = d.prefabs.filter((p) => p.id !== prefabId); }, { label: 'Delete prefab', source: 'prefabs' });
  }

  renamePrefab(prefabId, name) {
    this.mutateDoc((d) => { const p = d.prefabs.find((x) => x.id === prefabId); if (p && name) p.name = name; }, { label: 'Rename prefab', source: 'prefabs' });
  }

  getEntity(id) {
    return this.project?.objects.find((o) => o.id === id) || null;
  }

  get selectedId() {
    return this.selection[this.selection.length - 1] || null;
  }

  get selected() {
    return this.getEntity(this.selectedId);
  }

  get selectedEntities() {
    return this.selection.map((id) => this.getEntity(id)).filter(Boolean);
  }

  // ---------- editing helpers ----------
  /** Where new things go: in front of the camera, on the ground. */
  dropPoint() {
    const t = this.orbit.target;
    const jitter = () => (Math.random() - 0.5) * 1.5;
    return [round(t.x + jitter(), 2), 0, round(t.z + jitter(), 2)];
  }

  addPrefab(key) {
    const p = prefabToEntity(key);
    if (!p) return null;
    const entity = createEntity(p.overrides.shape, p.overrides);
    const [x, , z] = this.dropPoint();
    entity.position = [x, round(halfHeight(entity) + p.lift, 3), z];
    this.mutate((d) => {
      if (p.unique) d.objects = d.objects.filter((o) => o.shape !== entity.shape);
      d.objects.push(entity);
    }, { label: `Add ${entity.name}` });
    this.select(entity.id);
    return entity;
  }

  updateEntity(id, patch, source = 'inspector') {
    this.mutate((d) => {
      const i = d.objects.findIndex((o) => o.id === id);
      if (i < 0) return;
      d.objects[i] = normalizeEntity({ ...d.objects[i], ...patch, id });
    }, { source, label: 'Change object' });
  }

  updateEntities(ids, patchFn, source = 'inspector', label = 'Change objects') {
    const set = new Set(ids);
    this.mutate((d) => {
      d.objects = d.objects.map((o) => (set.has(o.id) ? normalizeEntity({ ...o, ...patchFn(o), id: o.id }) : o));
    }, { source, label });
  }

  setEnvironment(patch, source = 'inspector') {
    this.mutate((d) => { d.environment = normalizeEnvironment({ ...d.environment, ...patch }); }, { source, label: 'Change world' });
  }

  setSettings(patch, source = 'inspector') {
    this.mutate((d) => { d.settings = normalizeSettings({ ...d.settings, ...patch }); }, { source, label: 'Change rules' });
  }

  rename(name) {
    this.mutate((d) => { d.name = String(name || '').trim().slice(0, 80) || 'My Game'; }, { source: 'title', label: 'Rename' });
  }

  duplicateSelected() {
    const ents = this.selectedEntities.filter((e) => e.shape !== 'spawn');
    if (!ents.length) return;
    const copies = ents.map((e) => {
      const copy = deepClone(e);
      copy.id = newId();
      copy.name = /\bcopy\b/i.test(e.name) ? e.name : `${e.name} copy`;
      copy.position = [e.position[0] + 1.5, e.position[1], e.position[2] + 1.5];
      return copy;
    });
    this.mutate((d) => { d.objects.push(...copies); }, { label: 'Duplicate' });
    this.selectMany(copies.map((c) => c.id));
  }

  deleteSelected() {
    const ids = new Set(this.selection);
    if (!ids.size) return;
    this.select(null);
    this.mutate((d) => { d.objects = d.objects.filter((o) => !ids.has(o.id)); }, { label: 'Delete' });
  }

  /** Moves the selected objects down until they rest on whatever is below them. */
  dropSelectedToFloor() {
    const ents = this.selectedEntities;
    if (!ents.length) return;
    const others = [...this.view.objects.entries()].filter(([id]) => !this.selection.includes(id)).map(([, o]) => o);
    const targets = [...others, ...(this.view.ground.visible ? [this.view.ground] : [])];
    const updates = new Map();
    for (const e of ents) {
      const obj = this.view.objects.get(e.id);
      if (!obj) continue;
      const box = new THREE.Box3().setFromObject(obj);
      const c = box.getCenter(new THREE.Vector3());
      this.raycaster.set(new THREE.Vector3(c.x, box.min.y - 0.001, c.z), new THREE.Vector3(0, -1, 0));
      this.raycaster.far = 5000;
      const hit = this.raycaster.intersectObjects(targets, true)[0];
      const floor = hit ? hit.point.y : 0;
      updates.set(e.id, round(e.position[1] - (box.min.y - floor), 3));
    }
    this.updateEntities([...updates.keys()], (o) => ({ position: [o.position[0], updates.get(o.id), o.position[2]] }), 'inspector', 'Drop to floor');
  }

  // ---------- selection ----------
  /** opts.add keeps the current selection, opts.toggle adds or removes. */
  select(id, { add = false, toggle = false } = {}) {
    if (!id || !this.getEntity(id)) {
      if (!add && !toggle) this.selection = [];
    } else if (toggle) {
      this.selection = this.selection.includes(id) ? this.selection.filter((x) => x !== id) : [...this.selection, id];
    } else if (add) {
      this.selection = [...this.selection.filter((x) => x !== id), id];
    } else {
      this.selection = [id];
    }
    this.refreshSelection();
    this.emit('select', this.selectedId);
  }

  selectMany(ids) {
    this.selection = ids.filter((id) => this.getEntity(id));
    this.refreshSelection();
    this.emit('select', this.selectedId);
  }

  selectAll() {
    this.selectMany(this.project.objects.map((o) => o.id));
  }

  refreshSelection() {
    for (const b of this.boxes) { this.scene.remove(b); b.geometry.dispose(); }
    this.boxes = [];
    const objs = this.selection.map((id) => this.view.objects.get(id)).filter(Boolean);
    for (const obj of objs.slice(0, 60)) {
      const box = new THREE.BoxHelper(obj, objs.length > 1 && obj !== objs[objs.length - 1] ? 0xffd58a : 0xffb547);
      box.material.depthTest = false;
      box.material.transparent = true;
      box.renderOrder = 999;
      box.userData.target = obj;
      this.scene.add(box);
      this.boxes.push(box);
    }
    if (objs.length === 1) {
      this.gizmo.attach(objs[0]);
    } else if (objs.length > 1) {
      const c = new THREE.Vector3();
      for (const o of objs) c.add(o.position);
      c.divideScalar(objs.length);
      this.pivot.position.copy(c);
      this.pivot.quaternion.identity();
      this.pivot.scale.set(1, 1, 1);
      this.pivot.updateMatrixWorld();
      this.gizmo.attach(this.pivot);
    } else {
      this.gizmo.detach();
    }
    this.gizmo.setMode(this.gizmoMode);
    this.gizmo.setSpace(objs.length > 1 ? 'world' : this.space);
  }

  setGizmoMode(mode) {
    this.gizmoMode = mode;
    this.gizmo.setMode(mode);
    this.emit('tool', mode);
  }

  cycleGizmoMode() {
    const order = ['translate', 'rotate', 'scale'];
    this.setGizmoMode(order[(order.indexOf(this.gizmoMode) + 1) % 3]);
  }

  setSnap(on) {
    this.snap = on;
    this.applySnap();
    this.emit('tool', this.gizmoMode);
  }

  setSnapValue(kind, value) {
    this.snapValues[kind] = value;
    this.applySnap();
    this.emit('tool', this.gizmoMode);
  }

  setSpace(space) {
    this.space = space;
    this.gizmo.setSpace(this.selection.length > 1 ? 'world' : space);
    this.emit('tool', this.gizmoMode);
  }

  applySnap() {
    this.gizmo.setTranslationSnap(this.snap ? this.snapValues.move : null);
    this.gizmo.setRotationSnap(this.snap ? this.snapValues.rotate * DEG : null);
    this.gizmo.setScaleSnap(this.snap ? this.snapValues.scale : null);
  }

  setViewMode(mode) {
    this.viewMode = mode;
    this.scene.overrideMaterial = mode === 'wireframe' ? this.wireMaterial : null;
    this.view.unlit = mode === 'unlit';
    this.view.envKey = '';
    this.view.applyEnvironment(this.project.environment);
    this.emit('tool', this.gizmoMode);
  }

  setGridVisible(on) {
    this.grid.visible = on;
    this.emit('tool', this.gizmoMode);
  }

  setNavigation(mode) {
    this.navigation = mode;
    const M = THREE.MOUSE;
    // Pro (Unreal style): right mouse flies, middle pans, left orbits. Simple: left orbits, right pans.
    this.orbit.mouseButtons = mode === 'unreal' ? { LEFT: M.ROTATE, MIDDLE: M.PAN, RIGHT: -1 } : { LEFT: M.ROTATE, MIDDLE: M.DOLLY, RIGHT: M.PAN };
  }

  readTransform(obj) {
    const e = new THREE.Euler().setFromQuaternion(obj.quaternion, 'YXZ');
    return {
      position: obj.position.toArray().map((v) => round(v, 3)),
      rotation: [e.x, e.y, e.z].map((v) => round(v / DEG, 2)),
      scale: obj.scale.toArray().map((v) => round(Math.max(0.02, Math.abs(v)), 3)),
    };
  }

  beginGroupDrag() {
    if (this.gizmo.object !== this.pivot) return;
    this.pivot.updateMatrixWorld();
    const inv = this.pivot.matrixWorld.clone().invert();
    this.groupRel = this.selection.map((id) => {
      const obj = this.view.objects.get(id);
      if (!obj) return null;
      obj.updateMatrixWorld();
      return { obj, rel: inv.clone().multiply(obj.matrixWorld) };
    }).filter(Boolean);
  }

  onGizmoMove() {
    const obj = this.gizmo.object;
    if (!obj) return;
    if (obj === this.pivot && this.groupRel) {
      this.pivot.updateMatrixWorld();
      for (const { obj: o, rel } of this.groupRel) {
        _m.multiplyMatrices(this.pivot.matrixWorld, rel);
        _m.decompose(o.position, o.quaternion, o.scale);
      }
    }
    for (const b of this.boxes) b.setFromObject(b.userData.target);
    const primary = this.view.objects.get(this.selectedId);
    if (primary) this.emit('transform', { id: this.selectedId, ...this.readTransform(primary) });
  }

  onGizmoRelease() {
    const obj = this.gizmo.object;
    if (!obj) return;
    if (obj === this.pivot) {
      const t = new Map(this.selection.map((id) => [id, this.view.objects.get(id)]).filter(([, o]) => o).map(([id, o]) => [id, this.readTransform(o)]));
      this.groupRel = null;
      this.updateEntities([...t.keys()], (o) => t.get(o.id), 'gizmo', 'Move objects');
      return;
    }
    const id = obj.userData.entityId;
    this.updateEntity(id, this.readTransform(obj), 'gizmo');
  }

  focusSelected() {
    const objs = this.selection.map((id) => this.view.objects.get(id)).filter(Boolean);
    const box = new THREE.Box3();
    if (objs.length) for (const o of objs) box.expandByObject(o);
    else {
      for (const o of this.view.objects.values()) box.expandByObject(o);
      if (box.isEmpty()) box.set(new THREE.Vector3(-5, 0, -5), new THREE.Vector3(5, 2, 5));
    }
    const center = box.getCenter(new THREE.Vector3());
    const size = box.getSize(new THREE.Vector3()).length();
    const dir = this.camera.position.clone().sub(this.orbit.target).normalize();
    const dist = Math.max(4, size * 1.3);
    this.orbit.target.copy(center);
    this.camera.position.copy(center).addScaledVector(dir, dist);
    this.orbit.update();
  }

  /** Where "Play from here" should put the player: below the camera, facing the same way. */
  cameraSpawn() {
    const dir = new THREE.Vector3();
    this.camera.getWorldDirection(dir);
    const p = this.camera.position.clone();
    this.raycaster.set(p, new THREE.Vector3(0, -1, 0));
    this.raycaster.far = 500;
    const targets = [...this.view.objects.values(), ...(this.view.ground.visible ? [this.view.ground] : [])];
    const hit = this.raycaster.intersectObjects(targets, true)[0];
    const y = hit ? hit.point.y + 0.6 : p.y;
    return { position: [p.x, y, p.z], yaw: Math.atan2(-dir.x, -dir.z) };
  }

  // ---------- pointer picking ----------
  bindPointer() {
    let down = null;
    this.canvas.addEventListener('pointerdown', (e) => {
      if (!this.active || e.button !== 0) return;
      down = { x: e.clientX, y: e.clientY };
    });
    this.canvas.addEventListener('pointerup', (e) => {
      if (!this.active || !down || e.button !== 0) return;
      const moved = Math.hypot(e.clientX - down.x, e.clientY - down.y);
      down = null;
      if (moved > 5 || this.gizmo.dragging || this.dragging) return;
      if (this.gizmo.axis) return; // clicked the gizmo itself
      const hit = this.pick(e);
      const additive = e.ctrlKey || e.metaKey || e.shiftKey;
      if (!hit && additive) return;
      this.select(hit, { toggle: additive });
    });
    this.canvas.addEventListener('dblclick', () => this.active && this.focusSelected());
    this.canvas.addEventListener('contextmenu', (e) => { if (this.navigation === 'unreal') e.preventDefault(); });
  }

  pick(e) {
    const r = this.canvas.getBoundingClientRect();
    this.pointer.set(((e.clientX - r.left) / r.width) * 2 - 1, -((e.clientY - r.top) / r.height) * 2 + 1);
    this.raycaster.setFromCamera(this.pointer, this.camera);
    this.raycaster.far = Infinity;
    const hits = this.raycaster.intersectObjects([...this.view.objects.values()], true);
    for (const h of hits) {
      let o = h.object;
      while (o && !o.userData.entityId) o = o.parent;
      if (o) return o.userData.entityId;
    }
    return null;
  }

  // ---------- Unreal-style fly camera (Pro) ----------
  bindFly() {
    this.flyKeys = new Set();
    this.flying = false;
    let last = null;
    const euler = new THREE.Euler(0, 0, 0, 'YXZ');
    this.canvas.addEventListener('pointerdown', (e) => {
      if (!this.active || this.navigation !== 'unreal' || e.button !== 2) return;
      this.flying = true;
      this.orbit.enabled = false;
      this.flyDistance = this.camera.position.distanceTo(this.orbit.target);
      euler.setFromQuaternion(this.camera.quaternion, 'YXZ');
      last = { x: e.clientX, y: e.clientY };
      try { this.canvas.setPointerCapture(e.pointerId); } catch { /* optional */ }
      this.emit('fly', true);
    });
    this.canvas.addEventListener('pointermove', (e) => {
      if (!this.flying || !last) return;
      euler.y -= (e.clientX - last.x) * 0.0035;
      euler.x = Math.max(-1.55, Math.min(1.55, euler.x - (e.clientY - last.y) * 0.0035));
      last = { x: e.clientX, y: e.clientY };
      this.camera.quaternion.setFromEuler(euler);
    });
    const stop = (e) => {
      if (!this.flying || (e.button !== undefined && e.button !== 2)) return;
      this.flying = false;
      last = null;
      this.flyKeys.clear();
      const fwd = new THREE.Vector3();
      this.camera.getWorldDirection(fwd);
      this.orbit.target.copy(this.camera.position).addScaledVector(fwd, Math.max(2, Math.min(this.flyDistance || 10, 40)));
      this.orbit.enabled = this.active && !this.dragging;
      this.orbit.update();
      this.emit('fly', false);
    };
    this.canvas.addEventListener('pointerup', stop);
    this.canvas.addEventListener('lostpointercapture', () => stop({}));
    this.canvas.addEventListener('wheel', (e) => {
      if (!this.flying) return;
      e.preventDefault();
      e.stopImmediatePropagation();
      this.flySpeed = Math.max(1, Math.min(200, this.flySpeed * (e.deltaY < 0 ? 1.2 : 1 / 1.2)));
      this.emit('tool', this.gizmoMode);
    }, { passive: false, capture: true });
    window.addEventListener('keydown', (e) => { if (this.flying) { this.flyKeys.add(e.code); e.preventDefault(); } });
    window.addEventListener('keyup', (e) => this.flyKeys.delete(e.code));
  }

  updateFly(dt) {
    if (!this.flying || !this.flyKeys.size) return;
    const k = this.flyKeys;
    const move = new THREE.Vector3((k.has('KeyD') ? 1 : 0) - (k.has('KeyA') ? 1 : 0), (k.has('KeyE') ? 1 : 0) - (k.has('KeyQ') ? 1 : 0), (k.has('KeyS') ? 1 : 0) - (k.has('KeyW') ? 1 : 0));
    if (!move.lengthSq()) return;
    const speed = this.flySpeed * (k.has('ShiftLeft') || k.has('ShiftRight') ? 3 : 1);
    move.normalize().multiplyScalar(speed * dt);
    const up = move.y;
    move.y = 0;
    move.applyQuaternion(this.camera.quaternion);
    move.y += up;
    this.camera.position.add(move);
  }

  // ---------- loop ----------
  frame(dt = 0) {
    this.updateFly(dt);
    this.view.tick(dt);
    if (!this.flying) this.orbit.update();
    if (this.boxes.length && !this.dragging) for (const b of this.boxes) b.setFromObject(b.userData.target);
    this.view.updateSun(this.orbit.target);
    const env = this.project?.environment;
    if (env) {
      const s = Math.max(80, env.groundSize);
      if (this.grid.userData.size !== s) {
        this.grid.userData.size = s;
        this.grid.scale.setScalar(s / 80);
      }
    }
  }

  resize(w, h) {
    this.camera.aspect = w / Math.max(1, h);
    this.camera.updateProjectionMatrix();
  }

  render(renderer) {
    renderer.render(this.scene, this.camera);
  }

  setActive(on) {
    this.active = on;
    this.orbit.enabled = on;
    this.gizmo.enabled = on;
    this.gizmo.getHelper().visible = on && !!this.gizmo.object;
    for (const b of this.boxes) b.visible = on;
  }
}
