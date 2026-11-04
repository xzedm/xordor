// Renders a small picture of every prefab for the Add panel, using the real
// 3D shapes, so the pictures always match what you get.
import * as THREE from 'three';
import { RoomEnvironment } from 'three/addons/environments/RoomEnvironment.js';
import { PREFAB_GROUPS, prefabToEntity } from '../core/prefabs.js';
import { createEntity } from '../core/project.js';
import { buildShape } from '../core/shapes.js';
import { materialFactory, textTexture } from '../engine/materials.js';

const cache = new Map();

export function renderThumbnails(size = 112) {
  if (cache.size) return Promise.resolve(cache);
  return new Promise((resolve) => {
    const run = () => {
      let renderer;
      try {
        renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true, preserveDrawingBuffer: true });
      } catch {
        resolve(cache);
        return;
      }
      renderer.setPixelRatio(1);
      renderer.setSize(size, size, false);
      renderer.setClearColor(0x000000, 0);
      const scene = new THREE.Scene();
      const pmrem = new THREE.PMREMGenerator(renderer);
      scene.environment = pmrem.fromScene(new RoomEnvironment(), 0.04).texture;
      scene.environmentIntensity = 0.5;
      scene.add(new THREE.HemisphereLight(0xffffff, 0x667788, 2.2));
      const sun = new THREE.DirectionalLight(0xffffff, 2.2);
      sun.position.set(3, 5, 4);
      scene.add(sun);
      const camera = new THREE.PerspectiveCamera(30, 1, 0.1, 100);
      for (const group of PREFAB_GROUPS) {
        for (const item of group.items) {
          try {
            const { overrides } = prefabToEntity(item.key);
            const entity = createEntity(overrides.shape, overrides);
            entity.scale = [1, 1, 1].map((_, i) => entity.scale[i]);
            const obj = buildShape(entity, { mat: materialFactory(entity), textTexture });
            obj.scale.set(...entity.scale);
            obj.rotation.set(0, item.shape === 'spawn' ? 0 : -0.5, 0);
            scene.add(obj);
            const box = new THREE.Box3().setFromObject(obj);
            const sphere = box.getBoundingSphere(new THREE.Sphere());
            const dist = sphere.radius / Math.sin((30 * Math.PI) / 360) * 1.05;
            const dir = new THREE.Vector3(0.9, 0.75, 1.3).normalize();
            camera.position.copy(sphere.center).addScaledVector(dir, dist);
            camera.lookAt(sphere.center);
            renderer.render(scene, camera);
            cache.set(item.key, renderer.domElement.toDataURL('image/png'));
            scene.remove(obj);
            obj.traverse((o) => o.geometry?.dispose());
          } catch {
            /* a missing thumbnail just shows the label */
          }
        }
      }
      pmrem.dispose();
      scene.environment.dispose();
      renderer.dispose();
      renderer.forceContextLoss?.();
      resolve(cache);
    };
    (window.requestIdleCallback || ((fn) => setTimeout(fn, 60)))(run);
  });
}
