// Shooting: the player's weapon, enemy shots, bullets, damage and explosions.
// Bullets are light (no physics bodies). Each step they are tested against
// destroyable objects, the player, and solid things in the physics world.
import * as THREE from 'three';
import { CANNON } from './physics.js';
import { WEAPONS } from '../core/project.js';
import { logger } from '../core/log.js';

export const PLAYER_GROUP = 2;
const MAX_BULLETS = 300;
const MAX_PARTICLES = 400;
const _v = new THREE.Vector3();

export class Combat {
  constructor(game) {
    this.game = game;
    const w = WEAPONS[game.settings.weapon];
    this.weapon = w && w.rate ? w : null;
    this.bullets = [];
    this.particles = [];
    this.cooldown = 0;
    this.bulletGeo = new THREE.SphereGeometry(1, 10, 8);
    this.particleGeo = new THREE.BoxGeometry(1, 1, 1);
    this.materials = new Map();
    this.ray = new CANNON.RaycastResult();
    this.from = new CANNON.Vec3();
    this.to = new CANNON.Vec3();
    this.aim = new THREE.Raycaster();
  }

  get enabled() {
    return !!this.weapon;
  }

  material(color) {
    if (!this.materials.has(color)) this.materials.set(color, new THREE.MeshBasicMaterial({ color, toneMapped: false }));
    return this.materials.get(color);
  }

  /** Things that can still be destroyed. */
  targets() {
    return this.game.runtime.filter((rt) => rt.hp > 0 && rt.alive && !rt.dead);
  }

  remaining() {
    return this.game.runtime.filter((rt) => rt.maxHp > 0 && !rt.destroyed && !rt.dead).length;
  }

  update(h) {
    const g = this.game;
    if (this.cooldown > 0) this.cooldown -= h;
    if (this.enabled && g.state === 'playing' && !g.driving && g.input.firing && this.cooldown <= 0) {
      this.cooldown = 1 / this.weapon.rate;
      this.firePlayer();
    }
    this.stepBullets(h);
    this.stepParticles(h);
  }

  firePlayer() {
    const g = this.game;
    const w = this.weapon;
    const muzzle = g.playerPosition.clone();
    muzzle.y += 0.35;
    const dir = this.aimDirection(muzzle);
    if (w.spread) {
      dir.x += (Math.random() - 0.5) * w.spread * 2;
      dir.y += (Math.random() - 0.5) * w.spread * 2;
      dir.z += (Math.random() - 0.5) * w.spread * 2;
      dir.normalize();
    }
    muzzle.addScaledVector(dir, 0.6);
    g.facing = Math.atan2(dir.x, dir.z);
    this.spawnBullet({ pos: muzzle, dir, speed: w.speed, damage: w.damage, radius: w.radius, color: w.color, owner: 'player', knock: w.knock });
    g.sound('shoot');
  }

  /** Aims through the middle of the screen, like a crosshair. */
  aimDirection(muzzle) {
    const g = this.game;
    if (g.settings.camera === 'top-down') return new THREE.Vector3(Math.sin(g.facing), 0, Math.cos(g.facing));
    const cam = g.camera;
    const dir = new THREE.Vector3();
    cam.getWorldDirection(dir);
    this.aim.set(cam.position, dir);
    this.aim.far = 400;
    const skip = cam.position.distanceTo(g.playerPosition) + 0.4;
    const hits = this.aim.intersectObjects([...g.cameraBlockers, ...this.targets().map((rt) => rt.obj)], true);
    const hit = hits.find((x) => x.distance > skip && x.object.visible !== false);
    const point = hit ? hit.point : cam.position.clone().addScaledVector(dir, 200);
    return point.sub(muzzle).normalize();
  }

  /** An enemy (turret, robot...) fires at the player. */
  fireEnemy(rt, target, p) {
    const from = rt.obj.position.clone();
    const dir = _v.copy(target).sub(from).normalize().clone();
    const bounds = rt.obj.userData.localBounds;
    const reach = bounds ? bounds.max.distanceTo(bounds.min) * 0.5 * Math.max(rt.obj.scale.x, rt.obj.scale.y, rt.obj.scale.z) : 0.8;
    from.addScaledVector(dir, reach + 0.35);
    this.spawnBullet({ pos: from, dir, speed: p.speed, damage: p.damage, radius: 0.17, color: p.color, owner: 'enemy', source: rt, knock: 3 });
    this.game.sound('zap');
  }

  spawnBullet(b) {
    if (this.bullets.length >= MAX_BULLETS) this.removeBullet(0);
    const mesh = new THREE.Mesh(this.bulletGeo, this.material(b.color));
    mesh.scale.setScalar(b.radius);
    mesh.position.copy(b.pos);
    this.game.scene.add(mesh);
    this.bullets.push({ ...b, mesh, pos: b.pos.clone(), vel: b.dir.clone().multiplyScalar(b.speed), life: 3 });
  }

  removeBullet(i) {
    const b = this.bullets[i];
    this.game.scene.remove(b.mesh);
    this.bullets.splice(i, 1);
  }

  stepBullets(h) {
    for (let i = this.bullets.length - 1; i >= 0; i--) {
      const b = this.bullets[i];
      b.life -= h;
      const from = b.pos.clone();
      b.pos.addScaledVector(b.vel, h);
      if (b.life <= 0 || this.checkHit(b, from)) this.removeBullet(i);
      else b.mesh.position.copy(b.pos);
    }
  }

  checkHit(b, from) {
    const g = this.game;
    const samples = [from.clone().lerp(b.pos, 0.5), b.pos];
    if (b.owner === 'player') {
      for (const rt of this.targets()) {
        for (const p of samples) {
          if (g.distanceToObject(rt, p) <= b.radius + 0.05) {
            this.damage(rt, b.damage, b);
            this.burst(p, b.color, 8, 4);
            return true;
          }
        }
      }
    } else {
      for (const p of samples) {
        if (p.distanceTo(g.playerPosition) <= g.hitRadius + b.radius) {
          g.hurt(b.damage, b.source);
          this.burst(p, b.color, 10, 4);
          return true;
        }
      }
    }
    // Solid things: walls, the ground, crates...
    this.from.set(from.x, from.y, from.z);
    this.to.set(b.pos.x, b.pos.y, b.pos.z);
    this.ray.reset();
    g.world.raycastClosest(this.from, this.to, { collisionFilterMask: b.owner === 'player' ? ~PLAYER_GROUP : -1, skipBackfaces: true }, this.ray);
    if (!this.ray.hasHit) return false;
    const body = this.ray.body;
    const hp = this.ray.hitPointWorld;
    const point = new THREE.Vector3(hp.x, hp.y, hp.z);
    if (b.owner === 'enemy' && (body === g.playerBody || body === g.driving?.body)) {
      g.hurt(b.damage, b.source);
    } else {
      const rt = g.bodyOwner.get(body);
      if (rt && b.owner === 'player' && rt.hp > 0) this.damage(rt, b.damage, b);
    }
    if (body.type === CANNON.Body.DYNAMIC && b.knock) {
      const d = b.vel.clone().normalize().multiplyScalar(b.knock);
      body.wakeUp();
      body.applyImpulse(new CANNON.Vec3(d.x, d.y, d.z), hp);
    }
    this.burst(point, b.color, 6, 3);
    return true;
  }

  damage(rt, amount, bullet = null) {
    if (!(rt.hp > 0)) return;
    const g = this.game;
    rt.hp -= amount;
    rt.hitPulse = 0.12;
    if (rt.hp > 0) {
      g.sound('hit');
      return;
    }
    rt.hp = 0;
    rt.destroyed = true;
    const p = rt.healthParams || {};
    const center = rt.obj.position.clone();
    const color = `#${new THREE.Color(rt.entity.color).getHexString()}`;
    if (p.effect === 'explode') {
      this.burst(center, color, 26, 9);
      this.burst(center, '#ffb347', 18, 7);
      g.sound('explode');
    } else if (p.effect === 'pop') {
      this.burst(center, color, 14, 5);
      g.sound('pop');
    }
    g.setAlive(rt, false);
    if (p.points) g.score += p.points;
    logger.info('game', `Destroyed "${rt.entity.name}"${p.points ? ` (+${p.points})` : ''}.`);
    g.updateHud();
    if (bullet?.owner === 'player') g.onDestroyed?.(rt);
    if (g.winCondition === 'defeat-all' && this.remaining() === 0) g.endGame(true, 'You win!', 'You defeated every enemy.');
    if (g.winCondition === 'score' && g.score >= g.settings.targetScore) g.endGame(true, 'You win!', `You reached ${g.settings.targetScore} points.`);
  }

  /** A small shower of glowing cubes. */
  burst(at, color, count, speed) {
    const mat = this.material(color);
    for (let i = 0; i < count; i++) {
      if (this.particles.length >= MAX_PARTICLES) this.removeParticle(0);
      const m = new THREE.Mesh(this.particleGeo, mat);
      const size = 0.06 + Math.random() * 0.12;
      m.scale.setScalar(size);
      m.position.copy(at);
      this.game.scene.add(m);
      const v = new THREE.Vector3(Math.random() - 0.5, Math.random() * 0.9 + 0.1, Math.random() - 0.5).normalize().multiplyScalar(speed * (0.4 + Math.random() * 0.8));
      this.particles.push({ mesh: m, vel: v, life: 0.5 + Math.random() * 0.4, size, spin: (Math.random() - 0.5) * 12 });
    }
  }

  removeParticle(i) {
    this.game.scene.remove(this.particles[i].mesh);
    this.particles.splice(i, 1);
  }

  stepParticles(h) {
    const gravity = this.game.settings.gravity * 0.6;
    for (let i = this.particles.length - 1; i >= 0; i--) {
      const p = this.particles[i];
      p.life -= h;
      if (p.life <= 0) { this.removeParticle(i); continue; }
      p.vel.y -= gravity * h;
      p.mesh.position.addScaledVector(p.vel, h);
      p.mesh.rotation.x += p.spin * h;
      p.mesh.rotation.y += p.spin * h;
      p.mesh.scale.setScalar(p.size * Math.min(1, p.life * 2.5));
    }
  }

  dispose() {
    for (const b of this.bullets) this.game.scene.remove(b.mesh);
    for (const p of this.particles) this.game.scene.remove(p.mesh);
    this.bullets = [];
    this.particles = [];
    this.bulletGeo.dispose();
    this.particleGeo.dispose();
    for (const m of this.materials.values()) m.dispose();
  }
}
