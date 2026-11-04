// The shape library: every kind of thing you can put in a Xordor world.
//
// Each shape has friendly metadata (used by the editor and the AI prompt) and
// a `build` function that returns a THREE.Group made of simple parts. Parts can
// carry `userData.collider` so the physics engine knows how solid they are.
// After building, the group is centred so `position` is always the middle of
// the object and `size` describes its width / height / depth at scale 1.
import * as THREE from 'three';

const PI = Math.PI;

/** Creates one mesh part. `collider` is 'box' | 'sphere' | 'cylinder' | 'hull' | 'trimesh' | null. */
function part(geometry, material, { pos, rot, scale, collider = null, shadow = true } = {}) {
  const mesh = new THREE.Mesh(geometry, material);
  if (pos) mesh.position.set(...pos);
  if (rot) mesh.rotation.set(...rot);
  if (scale) mesh.scale.set(...scale);
  mesh.castShadow = shadow;
  mesh.receiveShadow = true;
  mesh.userData.collider = collider;
  return mesh;
}

function starShape(points = 5, outer = 0.5, inner = 0.22) {
  const s = new THREE.Shape();
  for (let i = 0; i <= points * 2; i++) {
    const r = i % 2 === 0 ? outer : inner;
    const a = (i / (points * 2)) * PI * 2 + PI / 2;
    const x = Math.cos(a) * r;
    const y = Math.sin(a) * r;
    if (i === 0) s.moveTo(x, y);
    else s.lineTo(x, y);
  }
  return s;
}

function heartShape() {
  const s = new THREE.Shape();
  s.moveTo(0, -0.42);
  s.bezierCurveTo(-0.15, -0.28, -0.5, -0.1, -0.5, 0.15);
  s.bezierCurveTo(-0.5, 0.36, -0.33, 0.48, -0.2, 0.48);
  s.bezierCurveTo(-0.08, 0.48, 0, 0.4, 0, 0.3);
  s.bezierCurveTo(0, 0.4, 0.08, 0.48, 0.2, 0.48);
  s.bezierCurveTo(0.33, 0.48, 0.5, 0.36, 0.5, 0.15);
  s.bezierCurveTo(0.5, -0.1, 0.15, -0.28, 0, -0.42);
  return s;
}

function extrude(shape, depth, bevel = 0.03) {
  const g = new THREE.ExtrudeGeometry(shape, {
    depth,
    bevelEnabled: bevel > 0,
    bevelThickness: bevel,
    bevelSize: bevel,
    bevelSegments: 2,
    curveSegments: 16,
  });
  g.center();
  return g;
}

/** Wedge: full height at the back (-z), sloping down to the front (+z). */
function rampGeometry() {
  const v = [
    [-0.5, -0.5, -0.5], [0.5, -0.5, -0.5], [0.5, -0.5, 0.5], [-0.5, -0.5, 0.5],
    [-0.5, 0.5, -0.5], [0.5, 0.5, -0.5],
  ];
  const faces = [[0, 1, 2, 3], [0, 4, 5, 1], [3, 2, 5, 4], [0, 3, 4], [1, 5, 2]];
  return polyGeometry(v, faces);
}

function polyGeometry(vertices, faces) {
  const pos = [];
  for (const f of faces) {
    for (let i = 1; i < f.length - 1; i++) {
      pos.push(...vertices[f[0]], ...vertices[f[i]], ...vertices[f[i + 1]]);
    }
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
  g.computeVertexNormals();
  // Simple planar UVs so patterns still show up.
  const uv = [];
  for (let i = 0; i < pos.length; i += 3) uv.push(pos[i] + 0.5, pos[i + 1] + pos[i + 2] + 0.5);
  g.setAttribute('uv', new THREE.Float32BufferAttribute(uv, 2));
  return g;
}

function hash(n) {
  const x = Math.sin(n * 127.1 + 311.7) * 43758.5453;
  return x - Math.floor(x);
}

function rockGeometry(seed = 1) {
  const g = new THREE.IcosahedronGeometry(0.5, 1);
  const p = g.attributes.position;
  const keyed = new Map();
  for (let i = 0; i < p.count; i++) {
    const key = `${p.getX(i).toFixed(3)},${p.getY(i).toFixed(3)},${p.getZ(i).toFixed(3)}`;
    if (!keyed.has(key)) keyed.set(key, 0.78 + hash(seed * 31 + keyed.size) * 0.34);
    const k = keyed.get(key);
    p.setXYZ(i, p.getX(i) * k, p.getY(i) * k * 0.78, p.getZ(i) * k);
  }
  g.computeVertexNormals();
  return g;
}

const WOOD = { color: '#8a5a33', look: 'matte', pattern: 'wood' };
const METAL_GREY = { color: '#9aa3ad', look: 'metal' };
const DARK = { color: '#2d2f36', look: 'plastic' };
const WHITE = { color: '#f4f4f4', look: 'plastic' };

export const SHAPES = {
  box: {
    label: 'Cube', size: [1, 1, 1],
    build: ({ mat }) => [part(new THREE.BoxGeometry(1, 1, 1), mat(), { collider: 'box' })],
  },
  sphere: {
    label: 'Ball', size: [1, 1, 1],
    build: ({ mat }) => [part(new THREE.SphereGeometry(0.5, 32, 20), mat(), { collider: 'sphere' })],
  },
  cylinder: {
    label: 'Cylinder', size: [1, 1, 1],
    build: ({ mat }) => [part(new THREE.CylinderGeometry(0.5, 0.5, 1, 32), mat(), { collider: 'cylinder' })],
  },
  cone: {
    label: 'Cone', size: [1, 1, 1],
    build: ({ mat }) => [part(new THREE.ConeGeometry(0.5, 1, 32), mat(), { collider: 'cylinder' })],
  },
  pyramid: {
    label: 'Pyramid', size: [1, 1, 1],
    build: ({ mat }) => {
      const g = new THREE.ConeGeometry(Math.SQRT1_2, 1, 4);
      g.rotateY(PI / 4);
      return [part(g, mat({ flat: true }), { collider: 'hull' })];
    },
  },
  capsule: {
    label: 'Capsule', size: [0.7, 1.3, 0.7],
    build: ({ mat }) => [part(new THREE.CapsuleGeometry(0.35, 0.6, 8, 20), mat(), { collider: 'capsule' })],
  },
  torus: {
    label: 'Ring', size: [1.04, 0.24, 1.04],
    build: ({ mat }) => {
      const g = new THREE.TorusGeometry(0.4, 0.12, 16, 48);
      g.rotateX(PI / 2);
      return [part(g, mat(), { collider: 'trimesh' })];
    },
  },
  ramp: {
    label: 'Ramp', size: [1, 1, 1],
    build: ({ mat }) => [part(rampGeometry(), mat({ flat: true }), { collider: 'hull' })],
  },
  stairs: {
    label: 'Stairs', size: [1, 1, 1],
    build: ({ mat }) => {
      const parts = [];
      const steps = 5;
      for (let i = 0; i < steps; i++) {
        const h = (i + 1) / steps;
        const d = 1 / steps;
        const z = 0.5 - d / 2 - i * d;
        parts.push(part(new THREE.BoxGeometry(1, h, d), mat(), { pos: [0, h / 2 - 0.5, z], collider: 'box' }));
      }
      return parts;
    },
  },
  coin: {
    label: 'Coin', size: [0.8, 0.8, 0.1],
    build: ({ mat }) => {
      const g = new THREE.CylinderGeometry(0.4, 0.4, 0.1, 40);
      g.rotateX(PI / 2);
      const face = new THREE.CylinderGeometry(0.28, 0.28, 0.12, 40);
      face.rotateX(PI / 2);
      return [part(g, mat()), part(face, mat({ dim: 0.82 }))];
    },
  },
  star: {
    label: 'Star', size: [1, 0.95, 0.2],
    build: ({ mat }) => [part(extrude(starShape(), 0.14), mat())],
  },
  heart: {
    label: 'Heart', size: [1, 0.9, 0.26],
    build: ({ mat }) => [part(extrude(heartShape(), 0.2), mat())],
  },
  gem: {
    label: 'Gem', size: [0.9, 1.26, 0.9],
    build: ({ mat }) => {
      const g = new THREE.OctahedronGeometry(0.45);
      g.scale(1, 1.4, 1);
      return [part(g, mat({ flat: true }))];
    },
  },
  key: {
    label: 'Key', size: [0.46, 0.95, 0.1],
    build: ({ mat }) => [
      part(new THREE.TorusGeometry(0.17, 0.055, 12, 32), mat(), { pos: [0, 0.27, 0] }),
      part(new THREE.BoxGeometry(0.09, 0.55, 0.08), mat(), { pos: [0, -0.15, 0] }),
      part(new THREE.BoxGeometry(0.16, 0.07, 0.08), mat(), { pos: [0.1, -0.33, 0] }),
      part(new THREE.BoxGeometry(0.12, 0.07, 0.08), mat(), { pos: [0.08, -0.2, 0] }),
    ],
  },
  flag: {
    label: 'Goal flag', size: [1.25, 2.5, 0.6],
    build: ({ mat }) => [
      part(new THREE.CylinderGeometry(0.3, 0.34, 0.1, 24), mat(METAL_GREY), { pos: [0, -1.2, 0] }),
      part(new THREE.CylinderGeometry(0.045, 0.045, 2.4, 12), mat(WHITE), { pos: [0, 0, 0] }),
      part(new THREE.BoxGeometry(0.9, 0.55, 0.03), mat(), { pos: [0.47, 0.88, 0] }),
      part(new THREE.SphereGeometry(0.08, 12, 8), mat({ color: '#ffd54a', look: 'metal' }), { pos: [0, 1.22, 0] }),
    ],
  },
  checkpoint: {
    label: 'Checkpoint', size: [0.8, 1.7, 0.6],
    build: ({ mat }) => {
      const tri = new THREE.Shape();
      tri.moveTo(0, 0); tri.lineTo(0.55, -0.18); tri.lineTo(0, -0.36); tri.lineTo(0, 0);
      const pennant = new THREE.ExtrudeGeometry(tri, { depth: 0.03, bevelEnabled: false });
      return [
        part(new THREE.CylinderGeometry(0.3, 0.3, 0.08, 24), mat(METAL_GREY), { pos: [0, -0.81, 0] }),
        part(new THREE.CylinderGeometry(0.04, 0.04, 1.6, 10), mat(WHITE), { pos: [0, 0, 0] }),
        part(pennant, mat(), { pos: [0.03, 0.78, -0.015] }),
      ];
    },
  },
  spawn: {
    label: 'Player start', size: [1.3, 0.12, 1.3], editorOnly: true,
    build: ({ mat }) => {
      const ring = new THREE.TorusGeometry(0.58, 0.06, 10, 48);
      ring.rotateX(PI / 2);
      const arrow = new THREE.ConeGeometry(0.16, 0.3, 3);
      arrow.rotateX(PI / 2);
      return [
        part(new THREE.CylinderGeometry(0.6, 0.6, 0.06, 40), mat({ dim: 0.6 }), { pos: [0, -0.03, 0] }),
        part(ring, mat({ look: 'glow' }), { pos: [0, 0, 0] }),
        part(arrow, mat({ look: 'glow' }), { pos: [0, 0.03, 0.32] }),
      ];
    },
  },
  light: {
    label: 'Light', size: [0.4, 0.4, 0.4],
    props: {
      brightness: { type: 'number', label: 'Brightness', default: 3, min: 0, max: 50, step: 0.5 },
      range: { type: 'number', label: 'Reach (meters)', default: 14, min: 1, max: 200, step: 1 },
    },
    build: ({ mat }) => [part(new THREE.SphereGeometry(0.2, 20, 14), mat({ look: 'glow' }), { shadow: false })],
  },
  enemy: {
    label: 'Enemy', size: [1.3, 1.3, 1.3],
    build: ({ mat }) => {
      const parts = [part(new THREE.SphereGeometry(0.5, 28, 18), mat(), { collider: 'sphere' })];
      const dirs = [[1, 0, 0], [-1, 0, 0], [0, 1, 0], [0, -1, 0], [0.7, 0.7, -0.2], [-0.7, 0.7, -0.2], [0, 0.6, -0.8], [0.6, -0.5, -0.6], [-0.6, -0.5, -0.6], [0, 0, -1]];
      for (const d of dirs) {
        const v = new THREE.Vector3(...d).normalize();
        const cone = new THREE.ConeGeometry(0.1, 0.32, 10);
        const m = part(cone, mat({ color: '#e8e8ea', look: 'metal' }));
        m.position.copy(v.clone().multiplyScalar(0.55));
        m.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), v);
        parts.push(m);
      }
      for (const x of [-0.17, 0.17]) {
        parts.push(part(new THREE.SphereGeometry(0.13, 16, 12), mat(WHITE), { pos: [x, 0.12, 0.43] }));
        parts.push(part(new THREE.SphereGeometry(0.065, 12, 10), mat({ color: '#111111' }), { pos: [x, 0.12, 0.54] }));
      }
      return parts;
    },
  },
  spikes: {
    label: 'Spikes', size: [1, 0.55, 1],
    build: ({ mat }) => {
      const parts = [part(new THREE.BoxGeometry(1, 0.1, 1), mat(DARK), { pos: [0, -0.225, 0], collider: 'box' })];
      for (let x = -1; x <= 1; x++) {
        for (let z = -1; z <= 1; z++) {
          parts.push(part(new THREE.ConeGeometry(0.13, 0.45, 12), mat(), { pos: [x * 0.31, 0.05, z * 0.31] }));
        }
      }
      return parts;
    },
  },
  bouncepad: {
    label: 'Bounce pad', size: [1.1, 0.26, 1.1],
    build: ({ mat }) => [
      part(new THREE.CylinderGeometry(0.55, 0.55, 0.16, 32), mat(DARK), { pos: [0, -0.05, 0], collider: 'cylinder' }),
      part(new THREE.CylinderGeometry(0.44, 0.44, 0.1, 32), mat({ look: 'glow' }), { pos: [0, 0.08, 0] }),
    ],
  },
  speedpad: {
    label: 'Speed pad', size: [1, 0.08, 1.4],
    build: ({ mat }) => [part(new THREE.BoxGeometry(1, 0.08, 1.4), mat({ pattern: 'arrows', look: 'glow' }), { collider: 'box' })],
  },
  portal: {
    label: 'Portal', size: [1.4, 1.4, 0.2],
    build: ({ mat }) => [
      part(new THREE.TorusGeometry(0.6, 0.1, 16, 48), mat({ look: 'glow' })),
      part(new THREE.CircleGeometry(0.55, 40), mat({ look: 'glass', double: true }), { shadow: false }),
    ],
  },
  tree: {
    label: 'Tree', size: [1.3, 2, 1.3],
    build: ({ mat }) => [
      part(new THREE.CylinderGeometry(0.11, 0.16, 1, 10), mat(WOOD), { pos: [0, -0.5, 0], collider: 'cylinder' }),
      part(new THREE.IcosahedronGeometry(0.65, 0), mat({ flat: true }), { pos: [0, 0.35, 0], collider: 'sphere' }),
    ],
  },
  pine: {
    label: 'Pine tree', size: [1.2, 2.4, 1.2],
    build: ({ mat }) => [
      part(new THREE.CylinderGeometry(0.1, 0.13, 0.6, 8), mat(WOOD), { pos: [0, -0.9, 0], collider: 'cylinder' }),
      part(new THREE.ConeGeometry(0.6, 0.9, 8), mat({ flat: true }), { pos: [0, -0.25, 0], collider: 'cylinder' }),
      part(new THREE.ConeGeometry(0.46, 0.8, 8), mat({ flat: true }), { pos: [0, 0.25, 0] }),
      part(new THREE.ConeGeometry(0.32, 0.7, 8), mat({ flat: true }), { pos: [0, 0.75, 0] }),
    ],
  },
  rock: {
    label: 'Rock', size: [1, 0.8, 1],
    props: { seed: { type: 'number', label: 'Shape variation', default: 1, min: 1, max: 999, step: 1 } },
    build: ({ mat, entity }) => [part(rockGeometry(entity?.props?.seed ?? 1), mat({ flat: true }), { collider: 'hull' })],
  },
  bush: {
    label: 'Bush', size: [1.3, 0.8, 1.1],
    build: ({ mat }) => [
      part(new THREE.IcosahedronGeometry(0.42, 1), mat({ flat: true }), { pos: [0, 0, 0], collider: 'sphere' }),
      part(new THREE.IcosahedronGeometry(0.3, 1), mat({ flat: true }), { pos: [0.38, -0.08, 0.05] }),
      part(new THREE.IcosahedronGeometry(0.28, 1), mat({ flat: true }), { pos: [-0.36, -0.1, -0.05] }),
    ],
  },
  cloud: {
    label: 'Cloud', size: [2, 0.9, 1.1],
    build: ({ mat }) => [
      part(new THREE.SphereGeometry(0.45, 20, 14), mat({ flat: false }), { pos: [0, 0.05, 0] }),
      part(new THREE.SphereGeometry(0.35, 20, 14), mat(), { pos: [0.55, -0.05, 0] }),
      part(new THREE.SphereGeometry(0.33, 20, 14), mat(), { pos: [-0.55, -0.07, 0.05] }),
      part(new THREE.SphereGeometry(0.3, 20, 14), mat(), { pos: [0.2, 0.25, 0.1] }),
    ],
  },
  house: {
    label: 'House', size: [2.3, 2.6, 2.3],
    build: ({ mat }) => {
      const roof = new THREE.ConeGeometry(1.62, 1, 4);
      roof.rotateY(PI / 4);
      return [
        part(new THREE.BoxGeometry(2, 1.6, 2), mat(), { pos: [0, -0.5, 0], collider: 'box' }),
        part(roof, mat({ color: '#b5533c', look: 'matte', flat: true }), { pos: [0, 0.8, 0], collider: 'hull' }),
        part(new THREE.BoxGeometry(0.5, 0.9, 0.05), mat(WOOD), { pos: [0, -0.85, 1.01] }),
        part(new THREE.BoxGeometry(0.42, 0.42, 0.05), mat({ color: '#bfe6ff', look: 'shiny' }), { pos: [-0.6, -0.3, 1.01] }),
        part(new THREE.BoxGeometry(0.42, 0.42, 0.05), mat({ color: '#bfe6ff', look: 'shiny' }), { pos: [0.6, -0.3, 1.01] }),
      ];
    },
  },
  sign: {
    label: 'Sign', size: [1.3, 1.5, 0.12],
    props: { text: { type: 'text', label: 'Sign text', default: 'Hello!' } },
    build: ({ mat, entity, textTexture }) => {
      const parts = [
        part(new THREE.BoxGeometry(0.1, 0.9, 0.1), mat(WOOD), { pos: [0, -0.3, -0.01], collider: 'box' }),
        part(new THREE.BoxGeometry(1.3, 0.75, 0.08), mat(), { pos: [0, 0.37, 0], collider: 'box' }),
      ];
      if (textTexture) {
        const tex = textTexture(entity?.props?.text ?? 'Hello!');
        const label = new THREE.Mesh(
          new THREE.PlaneGeometry(1.2, 0.65),
          new THREE.MeshBasicMaterial({ map: tex, transparent: true }),
        );
        label.position.set(0, 0.37, 0.045);
        parts.push(label);
      }
      return parts;
    },
  },
};

export const SHAPE_KEYS = Object.keys(SHAPES);

/**
 * Builds the visual for one entity. `ctx.mat(overrides)` must return a material.
 * Returns a centred THREE.Group with `userData.localBounds` set.
 */
export function buildShape(entity, ctx) {
  const def = SHAPES[entity.shape] || SHAPES.box;
  const group = new THREE.Group();
  const parts = def.build({ ...ctx, entity });
  for (const p of parts) group.add(p);
  group.updateMatrixWorld(true);
  const box = new THREE.Box3().setFromObject(group);
  const center = box.getCenter(new THREE.Vector3());
  if (center.lengthSq() > 1e-8) {
    for (const child of group.children) child.position.sub(center);
    box.translate(center.clone().negate());
  }
  group.userData.localBounds = box;
  return group;
}

/** Helpful text for the AI prompt. */
export function describeShapes() {
  return SHAPE_KEYS.map((k) => {
    const s = SHAPES[k];
    const props = s.props ? ` props: ${Object.entries(s.props).map(([pk, pv]) => `${pk} (${pv.type}, default ${JSON.stringify(pv.default)})`).join(', ')}` : '';
    return `- ${k}: ${s.label}, size ${s.size.join('x')} (w x h x d) at scale 1.${props}`;
  }).join('\n');
}
