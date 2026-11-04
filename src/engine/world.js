// WorldView turns project data into a Three.js scene: sky, sun, ground and objects.
// The editor and the game both use it, so what you build is exactly what you play.
import * as THREE from 'three';
import { buildShape, SHAPES } from '../core/shapes.js';
import { DEG } from '../core/util.js';
import { getMaterial, materialFactory, textTexture } from './materials.js';

const SKY_VERTEX = /* glsl */ `
  varying vec3 vWorld;
  void main() {
    vec4 wp = modelMatrix * vec4(position, 1.0);
    vWorld = wp.xyz;
    gl_Position = projectionMatrix * viewMatrix * wp;
  }
`;
const SKY_FRAGMENT = /* glsl */ `
  uniform vec3 top;
  uniform vec3 bottom;
  uniform vec3 sunDir;
  uniform vec3 sunColor;
  varying vec3 vWorld;
  void main() {
    vec3 dir = normalize(vWorld - cameraPosition);
    float h = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 col = mix(bottom, top, pow(smoothstep(0.35, 1.0, h), 0.8));
    float sun = pow(max(dot(dir, sunDir), 0.0), 600.0);
    float glow = pow(max(dot(dir, sunDir), 0.0), 12.0) * 0.25;
    col += sunColor * (sun * 2.0 + glow);
    gl_FragColor = vec4(col, 1.0);
    #include <colorspace_fragment>
  }
`;

export class WorldView {
  /** A shared reflection map (set once by the app) so metal and shiny things look right. */
  static environmentMap = null;

  constructor({ mode = 'edit' } = {}) {
    this.mode = mode;
    this.scene = new THREE.Scene();
    if (WorldView.environmentMap) this.scene.environment = WorldView.environmentMap;
    this.objects = new Map(); // id -> THREE.Group
    this.entityCache = new Map(); // id -> JSON string, to skip unchanged rebuilds

    this.hemi = new THREE.HemisphereLight(0xffffff, 0x445566, 1);
    this.scene.add(this.hemi);

    this.sun = new THREE.DirectionalLight(0xffffff, 2.5);
    this.sun.castShadow = true;
    this.sun.shadow.mapSize.set(2048, 2048);
    const sc = this.sun.shadow.camera;
    sc.left = sc.bottom = -45;
    sc.right = sc.top = 45;
    sc.near = 1;
    sc.far = 220;
    this.sun.shadow.bias = -0.0005;
    this.sun.shadow.normalBias = 0.03;
    this.scene.add(this.sun, this.sun.target);
    this.sunOffset = new THREE.Vector3(30, 60, 20);

    this.skyUniforms = {
      top: { value: new THREE.Color() },
      bottom: { value: new THREE.Color() },
      sunDir: { value: new THREE.Vector3(0, 1, 0) },
      sunColor: { value: new THREE.Color() },
    };
    this.sky = new THREE.Mesh(
      new THREE.SphereGeometry(900, 32, 16),
      new THREE.ShaderMaterial({
        uniforms: this.skyUniforms,
        vertexShader: SKY_VERTEX,
        fragmentShader: SKY_FRAGMENT,
        side: THREE.BackSide,
        depthWrite: false,
        fog: false,
      }),
    );
    this.sky.renderOrder = -10;
    this.sky.frustumCulled = false;
    this.scene.add(this.sky);

    this.stars = this.makeStars();
    this.scene.add(this.stars);

    this.ground = new THREE.Mesh(new THREE.PlaneGeometry(1, 1), new THREE.MeshStandardMaterial());
    this.ground.rotation.x = -Math.PI / 2;
    this.ground.receiveShadow = true;
    this.ground.userData.isGround = true;
    this.scene.add(this.ground);

    this.scene.fog = new THREE.Fog(0xffffff, 50, 400);
    this.envKey = '';
  }

  makeStars() {
    const g = new THREE.BufferGeometry();
    const pts = [];
    for (let i = 0; i < 1500; i++) {
      const v = new THREE.Vector3(Math.random() * 2 - 1, Math.random() * 0.9 + 0.1, Math.random() * 2 - 1).normalize().multiplyScalar(800);
      pts.push(v.x, v.y, v.z);
    }
    g.setAttribute('position', new THREE.Float32BufferAttribute(pts, 3));
    const p = new THREE.Points(g, new THREE.PointsMaterial({ color: 0xffffff, size: 2.2, sizeAttenuation: false, fog: false, transparent: true, opacity: 0.9 }));
    p.frustumCulled = false;
    return p;
  }

  applyEnvironment(env) {
    const key = JSON.stringify(env);
    if (key === this.envKey) return;
    this.envKey = key;
    this.skyUniforms.top.value.set(env.skyTop);
    this.skyUniforms.bottom.value.set(env.skyBottom);
    this.skyUniforms.sunColor.value.set(env.sunColor).multiplyScalar(Math.min(1, env.sunIntensity / 3));
    const elev = env.sunHeight * DEG;
    this.sunOffset.set(Math.cos(elev) * 0.6, Math.sin(elev), Math.cos(elev) * 0.8).normalize().multiplyScalar(90);
    this.skyUniforms.sunDir.value.copy(this.sunOffset).normalize();
    this.sun.color.set(env.sunColor);
    this.sun.intensity = env.sunIntensity;
    this.hemi.color.set(env.skyBottom).lerp(new THREE.Color('#ffffff'), 0.5);
    this.hemi.groundColor.set(env.groundColor).multiplyScalar(0.6);
    this.hemi.intensity = env.ambient * 1.4;
    this.scene.environmentIntensity = 0.15 + env.ambient * 0.3;
    this.stars.visible = !!env.stars;

    const fogAmount = env.fog;
    if (fogAmount <= 0.001) {
      this.scene.fog.near = 1e5; this.scene.fog.far = 1e5 + 1;
    } else {
      this.scene.fog.color.set(env.fogColor);
      this.scene.fog.near = 8 + (1 - fogAmount) * 90;
      this.scene.fog.far = 40 + (1 - fogAmount) ** 2 * 460;
    }

    this.ground.visible = env.showGround;
    this.ground.scale.set(env.groundSize, env.groundSize, 1);
    this.ground.material = getMaterial({ color: env.groundColor, look: 'matte', pattern: env.groundPattern, repeat: env.groundSize / 2 });
    this.updateSun(new THREE.Vector3());
  }

  /** Keeps the shadow area centred on what matters (the camera target or the player). */
  updateSun(focus) {
    this.sun.position.copy(focus).add(this.sunOffset);
    this.sun.target.position.copy(focus);
  }

  buildObject(entity) {
    const group = buildShape(entity, { mat: materialFactory(entity), textTexture });
    group.name = entity.name;
    group.userData.entityId = entity.id;
    group.userData.shape = entity.shape;
    if (entity.shape === 'light') {
      const light = new THREE.PointLight(entity.color, entity.props.brightness ?? 3, entity.props.range ?? 14, 1.6);
      light.castShadow = false;
      group.add(light);
    }
    if (entity.look === 'glass') group.traverse((o) => { o.castShadow = false; });
    this.applyTransform(group, entity);
    const hidden = !entity.visible || (this.mode === 'play' && SHAPES[entity.shape]?.editorOnly);
    group.visible = !hidden;
    if (this.mode === 'edit' && !entity.visible) {
      // Show hidden things faintly in the editor so they can still be found.
      group.visible = true;
      group.traverse((o) => {
        if (o.isMesh) {
          o.material = o.material.clone();
          o.material.transparent = true;
          o.material.opacity = 0.25;
          o.material.depthWrite = false;
        }
      });
    }
    return group;
  }

  applyTransform(obj, entity) {
    obj.position.set(...entity.position);
    obj.rotation.set(entity.rotation[0] * DEG, entity.rotation[1] * DEG, entity.rotation[2] * DEG, 'YXZ');
    obj.scale.set(...entity.scale);
  }

  addOrUpdate(entity) {
    const json = JSON.stringify(entity);
    if (this.entityCache.get(entity.id) === json) return this.objects.get(entity.id);
    const old = this.objects.get(entity.id);
    const prev = this.entityCache.get(entity.id);
    // Transform-only changes don't need a rebuild.
    if (old && prev) {
      const a = JSON.parse(prev);
      const sameLook = ['shape', 'color', 'look', 'pattern', 'visible', 'props', 'name'].every((k) => JSON.stringify(a[k]) === JSON.stringify(entity[k]));
      const scaleChanged = JSON.stringify(a.scale) !== JSON.stringify(entity.scale);
      if (sameLook && (!scaleChanged || entity.pattern === 'none')) {
        this.applyTransform(old, entity);
        this.entityCache.set(entity.id, json);
        return old;
      }
    }
    if (old) this.removeObject(entity.id);
    const obj = this.buildObject(entity);
    this.scene.add(obj);
    this.objects.set(entity.id, obj);
    this.entityCache.set(entity.id, json);
    return obj;
  }

  removeObject(id) {
    const obj = this.objects.get(id);
    if (!obj) return;
    this.scene.remove(obj);
    obj.traverse((o) => {
      if (o.isMesh) {
        o.geometry.dispose();
        if (o.material?.map?.isCanvasTexture && o.material.isMeshBasicMaterial) o.material.map.dispose();
      }
    });
    this.objects.delete(id);
    this.entityCache.delete(id);
  }

  /** Brings the scene in line with the project (adds, updates, removes objects). */
  sync(project) {
    this.applyEnvironment(project.environment);
    const ids = new Set();
    for (const e of project.objects) {
      ids.add(e.id);
      this.addOrUpdate(e);
    }
    for (const id of [...this.objects.keys()]) if (!ids.has(id)) this.removeObject(id);
  }

  dispose() {
    for (const id of [...this.objects.keys()]) this.removeObject(id);
    this.sky.geometry.dispose();
    this.stars.geometry.dispose();
  }
}
