// Imported 3D models (glTF / GLB, the open standard most 3D tools export).
// Models are decoded once and cached; every object that uses a model gets a
// cheap copy. Each copy is scaled so its longest side is 1 meter, then the
// object's own scale makes it bigger or smaller.
import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import { clone as cloneSkinned } from 'three/addons/utils/SkeletonUtils.js';
import { mergeGeometries } from 'three/addons/utils/BufferGeometryUtils.js';
import { RoomEnvironment } from 'three/addons/environments/RoomEnvironment.js';

const cache = new Map(); // asset id -> { promise, gltf, error }
const waiting = new Map(); // asset id -> Set of callbacks

function dataUrlToBuffer(url) {
  const comma = url.indexOf(',');
  const meta = url.slice(5, comma);
  const body = url.slice(comma + 1);
  if (!meta.includes(';base64')) return new TextEncoder().encode(decodeURIComponent(body)).buffer;
  const bin = atob(body);
  const bytes = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
  return bytes.buffer;
}

export function bufferToDataUrl(buffer, mime = 'model/gltf-binary') {
  const bytes = new Uint8Array(buffer);
  let bin = '';
  const chunk = 0x8000;
  for (let i = 0; i < bytes.length; i += chunk) bin += String.fromCharCode.apply(null, bytes.subarray(i, i + chunk));
  return `data:${mime};base64,${btoa(bin)}`;
}

/** Decodes a model file. Resolves with the glTF result or rejects with a friendly error. */
export function parseModel(buffer) {
  return new Promise((resolve, reject) => {
    const loader = new GLTFLoader();
    try {
      loader.parse(buffer, '', resolve, (err) => {
        const msg = String(err?.message || err);
        if (/draco|meshopt|KHR_draco|EXT_meshopt/i.test(msg)) reject(new Error('This model is compressed in a way Xordor cannot read yet. Export it again without Draco or Meshopt compression.'));
        else if (/external|uri|fetch|load/i.test(msg)) reject(new Error('This model needs extra files. Export it as a single .glb file instead.'));
        else reject(new Error(msg));
      });
    } catch (err) {
      reject(err);
    }
  });
}

/** Starts loading an asset (if needed) and resolves when it is ready. */
export function ensureModel(asset) {
  if (!asset) return Promise.reject(new Error('Missing model.'));
  const entry = cache.get(asset.id);
  if (entry) return entry.promise;
  const record = { gltf: null, error: null, promise: null };
  record.promise = parseModel(dataUrlToBuffer(asset.data)).then((gltf) => {
    record.gltf = gltf;
    for (const cb of waiting.get(asset.id) || []) cb(gltf);
    waiting.delete(asset.id);
    return gltf;
  }, (err) => {
    record.error = err;
    throw err;
  });
  cache.set(asset.id, record);
  return record.promise;
}

/** Stores an already decoded model (right after import) so it is not decoded twice. */
export function primeModel(asset, gltf) {
  cache.set(asset.id, { gltf, error: null, promise: Promise.resolve(gltf) });
  for (const cb of waiting.get(asset.id) || []) cb(gltf);
  waiting.delete(asset.id);
}

export function getModel(assetId) {
  return cache.get(assetId)?.gltf || null;
}

/** Calls `cb` once the model is ready (right away if it already is). */
export function whenModelReady(assetId, cb) {
  const gltf = getModel(assetId);
  if (gltf) { cb(gltf); return () => {}; }
  if (!waiting.has(assetId)) waiting.set(assetId, new Set());
  waiting.get(assetId).add(cb);
  return () => waiting.get(assetId)?.delete(cb);
}

export function forgetModel(assetId) {
  cache.delete(assetId);
  waiting.delete(assetId);
}

/** Geometry of every mesh under `root` (which has no parent), including root's own scale. */
function mergedGeometry(root, maxVertices = 400000) {
  root.updateMatrixWorld(true);
  const geos = [];
  let count = 0;
  root.traverse((o) => {
    if (!o.isMesh || !o.geometry?.attributes?.position) return;
    let g = o.geometry.index ? o.geometry.toNonIndexed() : o.geometry.clone();
    for (const name of Object.keys(g.attributes)) if (name !== 'position') g.deleteAttribute(name);
    g.morphAttributes = {};
    g.applyMatrix4(o.matrixWorld);
    count += g.attributes.position.count;
    if (count <= maxVertices) geos.push(g);
  });
  if (!geos.length) return null;
  return mergeGeometries(geos, false);
}

function sampleGeometry(geo, maxPoints = 1500) {
  const pos = geo.attributes.position;
  const step = Math.max(1, Math.floor(pos.count / maxPoints));
  const pts = [];
  for (let i = 0; i < pos.count; i += step) pts.push(pos.getX(i), pos.getY(i), pos.getZ(i));
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.Float32BufferAttribute(pts, 3));
  return g;
}

/**
 * Builds the parts for a "model" entity, or returns null while the model is loading.
 * The first part is the model itself; an invisible part carries the collision shape.
 */
export function buildModelParts(entity) {
  const gltf = getModel(entity.props?.asset);
  if (!gltf) return null;
  const copy = cloneSkinned(gltf.scene);
  copy.traverse((o) => {
    if (o.isMesh) {
      o.castShadow = true;
      o.receiveShadow = true;
      o.userData.collider = null;
      o.userData.sharedGeometry = true; // owned by the cached model, not this copy
    }
  });
  // Fit inside a 1 meter box, centred.
  const box = new THREE.Box3().setFromObject(copy);
  const size = box.getSize(new THREE.Vector3());
  const longest = Math.max(size.x, size.y, size.z) || 1;
  const holder = new THREE.Group();
  holder.add(copy);
  holder.scale.setScalar(1 / longest);
  const center = box.getCenter(new THREE.Vector3());
  copy.position.sub(center);
  holder.userData.isModel = true;

  const clips = gltf.animations || [];
  const wanted = String(entity.props?.animation || '').trim();
  if (clips.length && wanted.toLowerCase() !== 'none') {
    holder.userData.animation = { root: copy, clip: clips.find((c) => c.name === wanted) || clips[0] };
  }

  const parts = [holder];
  const kind = entity.props?.collider || 'box';
  if (kind !== 'none') {
    const invisible = new THREE.MeshBasicMaterial({ visible: false });
    let shape;
    if (kind === 'box') {
      shape = new THREE.Mesh(new THREE.BoxGeometry(size.x / longest, size.y / longest, size.z / longest), invisible);
      shape.userData.collider = 'box';
    } else {
      const merged = mergedGeometry(holder);
      if (merged) {
        shape = new THREE.Mesh(kind === 'hull' ? sampleGeometry(merged) : merged, invisible);
        shape.userData.collider = kind === 'hull' ? 'hull' : 'trimesh';
      }
    }
    if (shape) {
      shape.castShadow = false;
      shape.userData.colliderOnly = true;
      parts.push(shape);
    }
  }
  return parts;
}

/** Facts about a freshly imported model, for the editor. */
export function describeModel(gltf) {
  let triangles = 0;
  gltf.scene.traverse((o) => {
    if (o.isMesh && o.geometry) triangles += (o.geometry.index ? o.geometry.index.count : o.geometry.attributes.position.count) / 3;
  });
  return { animations: (gltf.animations || []).map((a) => a.name || 'Animation'), triangles: Math.round(triangles) };
}

/** A small picture of the model for the editor's panels. */
export function renderModelThumbnail(gltf, size = 128) {
  let renderer;
  try {
    renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true, preserveDrawingBuffer: true });
  } catch {
    return '';
  }
  try {
    renderer.setSize(size, size, false);
    renderer.setClearColor(0x000000, 0);
    renderer.toneMapping = THREE.ACESFilmicToneMapping;
    const scene = new THREE.Scene();
    const pmrem = new THREE.PMREMGenerator(renderer);
    scene.environment = pmrem.fromScene(new RoomEnvironment(), 0.04).texture;
    scene.add(new THREE.HemisphereLight(0xffffff, 0x667788, 1.6));
    const sun = new THREE.DirectionalLight(0xffffff, 2);
    sun.position.set(3, 5, 4);
    scene.add(sun);
    const copy = cloneSkinned(gltf.scene);
    scene.add(copy);
    const sphere = new THREE.Box3().setFromObject(copy).getBoundingSphere(new THREE.Sphere());
    const camera = new THREE.PerspectiveCamera(30, 1, sphere.radius / 100, sphere.radius * 100);
    camera.position.copy(sphere.center).addScaledVector(new THREE.Vector3(0.9, 0.6, 1.3).normalize(), (sphere.radius / Math.sin((30 * Math.PI) / 360)) * 1.05);
    camera.lookAt(sphere.center);
    renderer.render(scene, camera);
    const url = renderer.domElement.toDataURL('image/png');
    scene.environment.dispose();
    pmrem.dispose();
    return url;
  } catch {
    return '';
  } finally {
    renderer.dispose();
    renderer.forceContextLoss?.();
  }
}
