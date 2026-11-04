// The editor: owns the project data, undo history, selection and the 3D view
// you build in. Panels talk to it through small methods and "change" events.
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { TransformControls } from 'three/addons/controls/TransformControls.js';
import { WorldView } from '../engine/world.js';
import { createEntity, halfHeight, normalizeEntity, normalizeEnvironment, normalizeProject, normalizeSettings } from '../core/project.js';
import { prefabToEntity } from '../core/prefabs.js';
import { DEG, deepClone, newId, round } from '../core/util.js';

const MAX_HISTORY = 100;

export class Editor {
  constructor({ canvas, container }) {
    this.canvas = canvas;
    this.container = container;
    this.listeners = new Map();
    this.project = null;
    this.selectedId = null;
    this.undoStack = [];
    this.redoStack = [];
    this.gizmoMode = 'translate';
    this.snap = true;
    this.active = true;

    this.view = new WorldView({ mode: 'edit' });
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
    this.orbit.minDistance = 1.5;
    this.orbit.maxDistance = 600;
    this.orbit.screenSpacePanning = false;
    this.orbit.target.set(0, 1, -2);
    this.orbit.update();

    this.gizmo = new TransformControls(this.camera, canvas);
    this.gizmo.setSize(0.9);
    this.scene.add(this.gizmo.getHelper());
    this.gizmo.addEventListener('dragging-changed', (e) => {
      this.orbit.enabled = !e.value;
      this.dragging = e.value;
      if (e.value) this.dragStart = JSON.stringify(this.project);
    });
    this.gizmo.addEventListener('objectChange', () => this.onGizmoMove());
    this.gizmo.addEventListener('mouseUp', () => this.onGizmoRelease());
    this.applySnap();

    this.selectionBox = new THREE.BoxHelper(undefined, 0xffb547);
    this.selectionBox.material.depthTest = false;
    this.selectionBox.material.transparent = true;
    this.selectionBox.renderOrder = 999;
    this.selectionBox.visible = false;
    this.scene.add(this.selectionBox);

    this.raycaster = new THREE.Raycaster();
    this.pointer = new THREE.Vector2();
    this.bindPointer();
  }

  // ---------- events ----------
  on(type, fn) {
    if (!this.listeners.has(type)) this.listeners.set(type, new Set());
    this.listeners.get(type).add(fn);
    return () => this.listeners.get(type).delete(fn);
  }

  emit(type, detail) {
    for (const fn of this.listeners.get(type) || []) fn(detail);
  }

  // ---------- project & history ----------
  load(project, { keepHistory = false } = {}) {
    this.project = normalizeProject(project);
    if (!keepHistory) { this.undoStack = []; this.redoStack = []; }
    this.select(null);
    this.view.sync(this.project);
    this.grid.visible = true;
    this.emit('change', { source: 'load' });
  }

  /** Replaces the project with a modified copy, as one undo step. */
  commit(next, { source = 'edit', label = '' } = {}) {
    const before = JSON.stringify(this.project);
    const normalized = normalizeProject(next);
    const after = JSON.stringify(normalized);
    if (before === after) return false;
    this.undoStack.push({ json: before, label });
    if (this.undoStack.length > MAX_HISTORY) this.undoStack.shift();
    this.redoStack = [];
    this.project = normalized;
    this.view.sync(this.project);
    if (this.selectedId && !this.getEntity(this.selectedId)) this.select(null);
    else this.refreshSelection();
    this.emit('change', { source, label });
    return true;
  }

  /** Changes the project with a function that edits a draft copy. */
  mutate(fn, opts) {
    const draft = deepClone(this.project);
    fn(draft);
    return this.commit(draft, opts);
  }

  get historyDepth() {
    return this.undoStack.length;
  }

  undo() {
    const step = this.undoStack.pop();
    if (!step) return false;
    this.redoStack.push({ json: JSON.stringify(this.project), label: step.label });
    this.project = JSON.parse(step.json);
    this.view.sync(this.project);
    if (this.selectedId && !this.getEntity(this.selectedId)) this.select(null);
    else this.refreshSelection();
    this.emit('change', { source: 'undo' });
    return true;
  }

  redo() {
    const step = this.redoStack.pop();
    if (!step) return false;
    this.undoStack.push({ json: JSON.stringify(this.project), label: step.label });
    this.project = JSON.parse(step.json);
    this.view.sync(this.project);
    if (this.selectedId && !this.getEntity(this.selectedId)) this.select(null);
    else this.refreshSelection();
    this.emit('change', { source: 'redo' });
    return true;
  }

  getEntity(id) {
    return this.project?.objects.find((o) => o.id === id) || null;
  }

  get selected() {
    return this.getEntity(this.selectedId);
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
    const e = this.selected;
    if (!e) return;
    const copy = deepClone(e);
    copy.id = newId();
    copy.name = /\bcopy\b/i.test(e.name) ? e.name : `${e.name} copy`;
    copy.position = [e.position[0] + 1.5, e.position[1], e.position[2] + 1.5];
    this.mutate((d) => {
      if (copy.shape === 'spawn') return;
      d.objects.push(copy);
    }, { label: 'Duplicate' });
    if (copy.shape !== 'spawn') this.select(copy.id);
  }

  deleteSelected() {
    const id = this.selectedId;
    if (!id) return;
    this.select(null);
    this.mutate((d) => { d.objects = d.objects.filter((o) => o.id !== id); }, { label: 'Delete' });
  }

  // ---------- selection ----------
  select(id) {
    this.selectedId = id && this.getEntity(id) ? id : null;
    this.refreshSelection();
    this.emit('select', this.selectedId);
  }

  refreshSelection() {
    const obj = this.selectedId ? this.view.objects.get(this.selectedId) : null;
    if (obj) {
      this.gizmo.attach(obj);
      this.gizmo.setMode(this.gizmoMode);
      this.selectionBox.setFromObject(obj);
      this.selectionBox.visible = true;
    } else {
      this.gizmo.detach();
      this.selectionBox.visible = false;
    }
  }

  setGizmoMode(mode) {
    this.gizmoMode = mode;
    this.gizmo.setMode(mode);
    this.emit('tool', mode);
  }

  setSnap(on) {
    this.snap = on;
    this.applySnap();
    this.emit('tool', this.gizmoMode);
  }

  applySnap() {
    this.gizmo.setTranslationSnap(this.snap ? 0.25 : null);
    this.gizmo.setRotationSnap(this.snap ? 15 * DEG : null);
    this.gizmo.setScaleSnap(this.snap ? 0.1 : null);
  }

  readTransform(obj) {
    const e = new THREE.Euler().setFromQuaternion(obj.quaternion, 'YXZ');
    return {
      position: obj.position.toArray().map((v) => round(v, 3)),
      rotation: [e.x, e.y, e.z].map((v) => round(v / DEG, 2)),
      scale: obj.scale.toArray().map((v) => round(Math.max(0.02, Math.abs(v)), 3)),
    };
  }

  onGizmoMove() {
    const obj = this.gizmo.object;
    if (!obj) return;
    this.selectionBox.setFromObject(obj);
    this.emit('transform', { id: obj.userData.entityId, ...this.readTransform(obj) });
  }

  onGizmoRelease() {
    const obj = this.gizmo.object;
    if (!obj) return;
    const id = obj.userData.entityId;
    const t = this.readTransform(obj);
    this.updateEntity(id, t, 'gizmo');
  }

  focusSelected() {
    const obj = this.selectedId ? this.view.objects.get(this.selectedId) : null;
    const box = new THREE.Box3();
    if (obj) box.setFromObject(obj);
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
      this.select(hit);
    });
    this.canvas.addEventListener('dblclick', () => this.active && this.focusSelected());
  }

  pick(e) {
    const r = this.canvas.getBoundingClientRect();
    this.pointer.set(((e.clientX - r.left) / r.width) * 2 - 1, -((e.clientY - r.top) / r.height) * 2 + 1);
    this.raycaster.setFromCamera(this.pointer, this.camera);
    const roots = [...this.view.objects.values()];
    const hits = this.raycaster.intersectObjects(roots, true);
    for (const h of hits) {
      let o = h.object;
      while (o && !o.userData.entityId) o = o.parent;
      if (o) return o.userData.entityId;
    }
    return null;
  }

  // ---------- loop ----------
  frame() {
    this.orbit.update();
    if (this.selectionBox.visible && this.gizmo.object) this.selectionBox.setFromObject(this.gizmo.object);
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
    this.gizmo.getHelper().visible = on;
  }
}
