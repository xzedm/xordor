// Physics with cannon-es (open source, MIT). Colliders are built from the
// parts of each shape, so what you see is roughly what you bump into.
import * as CANNON from 'cannon-es';
import * as THREE from 'three';
import { ConvexHull } from 'three/addons/math/ConvexHull.js';

export { CANNON };

export function createPhysicsWorld(gravity) {
  const world = new CANNON.World({ gravity: new CANNON.Vec3(0, -gravity, 0) });
  world.broadphase = new CANNON.SAPBroadphase(world);
  world.allowSleep = true;
  world.solver.iterations = 10;
  world.defaultContactMaterial.friction = 0.4;
  world.defaultContactMaterial.restitution = 0.1;
  return world;
}

const _m = new THREE.Matrix3();

/** Scale of the root expressed in the child's own axes (exact for 90° turns). */
function localScale(child, rootScale) {
  _m.setFromMatrix4(new THREE.Matrix4().makeRotationFromQuaternion(child.quaternion));
  const e = _m.elements; // column-major
  const s = [rootScale.x, rootScale.y, rootScale.z];
  const out = [0, 0, 0];
  for (let i = 0; i < 3; i++) {
    // Column i of R is the child's axis i in root space.
    out[i] = Math.abs(e[i * 3]) * s[0] + Math.abs(e[i * 3 + 1]) * s[1] + Math.abs(e[i * 3 + 2]) * s[2];
  }
  return new THREE.Vector3(out[0], out[1], out[2]).multiply(child.scale);
}

function hullShape(geometry, scale) {
  const pos = geometry.attributes.position;
  const pts = [];
  const seen = new Set();
  for (let i = 0; i < pos.count; i++) {
    const key = `${pos.getX(i).toFixed(3)},${pos.getY(i).toFixed(3)},${pos.getZ(i).toFixed(3)}`;
    if (seen.has(key)) continue;
    seen.add(key);
    pts.push(new THREE.Vector3(pos.getX(i) * scale.x, pos.getY(i) * scale.y, pos.getZ(i) * scale.z));
  }
  const hull = new ConvexHull().setFromPoints(pts);
  const vertices = [];
  const index = new Map();
  const faces = [];
  for (const face of hull.faces) {
    const f = [];
    let edge = face.edge;
    do {
      const p = edge.head().point;
      const k = `${p.x.toFixed(4)},${p.y.toFixed(4)},${p.z.toFixed(4)}`;
      if (!index.has(k)) { index.set(k, vertices.length); vertices.push(new CANNON.Vec3(p.x, p.y, p.z)); }
      f.push(index.get(k));
      edge = edge.next;
    } while (edge !== face.edge);
    faces.push(f);
  }
  return new CANNON.ConvexPolyhedron({ vertices, faces });
}

function trimeshShape(geometry, scale) {
  const g = geometry.index ? geometry : geometry;
  const pos = g.attributes.position;
  const verts = [];
  for (let i = 0; i < pos.count; i++) verts.push(pos.getX(i) * scale.x, pos.getY(i) * scale.y, pos.getZ(i) * scale.z);
  const idx = g.index ? Array.from(g.index.array) : [...Array(pos.count).keys()];
  return new CANNON.Trimesh(verts, idx);
}

function addPart(body, child, rootScale, isStatic) {
  const kind = child.userData.collider;
  if (!kind || !child.geometry) return false;
  const geom = child.geometry;
  if (!geom.boundingBox) geom.computeBoundingBox();
  const s = localScale(child, rootScale);
  const bb = geom.boundingBox;
  const size = new THREE.Vector3().subVectors(bb.max, bb.min).multiply(s);
  const centerLocal = new THREE.Vector3().addVectors(bb.min, bb.max).multiplyScalar(0.5).multiply(child.scale).applyQuaternion(child.quaternion);
  const offset = child.position.clone().add(centerLocal).multiply(rootScale);
  const quat = new CANNON.Quaternion(child.quaternion.x, child.quaternion.y, child.quaternion.z, child.quaternion.w);
  const off = new CANNON.Vec3(offset.x, offset.y, offset.z);
  const tiny = 0.005;

  switch (kind) {
    case 'box':
      body.addShape(new CANNON.Box(new CANNON.Vec3(Math.max(tiny, size.x / 2), Math.max(tiny, size.y / 2), Math.max(tiny, size.z / 2))), off, quat);
      return true;
    case 'sphere':
      body.addShape(new CANNON.Sphere(Math.max(tiny, Math.max(size.x, size.y, size.z) / 2)), off, quat);
      return true;
    case 'cylinder': {
      const p = geom.parameters || {};
      const sxz = Math.max(s.x, s.z);
      const rt = p.radiusTop !== undefined ? p.radiusTop : 0;
      const rb = p.radiusBottom !== undefined ? p.radiusBottom : p.radius ?? 0.5;
      const h = (p.height ?? 1) * s.y;
      body.addShape(new CANNON.Cylinder(Math.max(0.02, rt * sxz), Math.max(0.02, rb * sxz), Math.max(tiny, h), 14), off, quat);
      return true;
    }
    case 'capsule': {
      const p = geom.parameters || {};
      const sxz = Math.max(s.x, s.z);
      const r = (p.radius ?? 0.35) * sxz;
      const len = (p.height ?? 0.6) * s.y;
      body.addShape(new CANNON.Cylinder(r, r, Math.max(tiny, len), 14), off, quat);
      const up = new THREE.Vector3(0, len / 2, 0).applyQuaternion(child.quaternion);
      body.addShape(new CANNON.Sphere(r), new CANNON.Vec3(off.x + up.x, off.y + up.y, off.z + up.z));
      body.addShape(new CANNON.Sphere(r), new CANNON.Vec3(off.x - up.x, off.y - up.y, off.z - up.z));
      return true;
    }
    case 'hull': {
      const shape = hullShape(geom, s);
      body.addShape(shape, new CANNON.Vec3(child.position.x * rootScale.x, child.position.y * rootScale.y, child.position.z * rootScale.z), quat);
      return true;
    }
    case 'trimesh':
      if (isStatic) {
        body.addShape(trimeshShape(geom, s), new CANNON.Vec3(child.position.x * rootScale.x, child.position.y * rootScale.y, child.position.z * rootScale.z), quat);
      } else {
        body.addShape(new CANNON.Cylinder(size.x / 2, size.x / 2, Math.max(tiny, size.y), 14), off, quat);
      }
      return true;
    default:
      return false;
  }
}

/**
 * Creates a body for an entity's built object. `type` is 'static' | 'dynamic' | 'kinematic'.
 * Shapes without solid parts fall back to their bounding box.
 */
export function createBody(entity, object, type, material) {
  const isStatic = type !== 'dynamic';
  const body = new CANNON.Body({
    mass: type === 'dynamic' ? entity.mass : 0,
    type: type === 'dynamic' ? CANNON.Body.DYNAMIC : type === 'kinematic' ? CANNON.Body.KINEMATIC : CANNON.Body.STATIC,
    material,
    linearDamping: 0.05,
    angularDamping: 0.1,
  });
  const rootScale = object.scale.clone();
  let any = false;
  for (const child of object.children) {
    if (child.isMesh) any = addPart(body, child, rootScale, type === 'static' && isStatic) || any;
  }
  if (!any) {
    const bb = object.userData.localBounds;
    const size = new THREE.Vector3().subVectors(bb.max, bb.min).multiply(rootScale);
    body.addShape(new CANNON.Box(new CANNON.Vec3(Math.max(0.01, size.x / 2), Math.max(0.01, size.y / 2), Math.max(0.01, size.z / 2))));
  }
  body.position.set(object.position.x, object.position.y, object.position.z);
  body.quaternion.set(object.quaternion.x, object.quaternion.y, object.quaternion.z, object.quaternion.w);
  if (type === 'dynamic') {
    body.sleepSpeedLimit = 0.15;
    body.sleepTimeLimit = 0.6;
  }
  return body;
}
