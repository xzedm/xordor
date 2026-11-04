// What each behavior actually does while the game is running.
// Every factory gets the runtime object (`rt`), its params and the game,
// and returns hooks: update(h, t), touch(), start().
import * as THREE from 'three';
import { DEG } from '../core/util.js';

const AXES = { x: new THREE.Vector3(1, 0, 0), y: new THREE.Vector3(0, 1, 0), z: new THREE.Vector3(0, 0, 1) };
const _q = new THREE.Quaternion();
const _v = new THREE.Vector3();

export const RUNTIME = {
  spin(rt, p) {
    let angle = 0;
    return {
      update(h) {
        angle += p.speed * DEG * h;
        _q.setFromAxisAngle(AXES[p.axis] || AXES.y, angle);
        rt.motion.spin.multiply(_q);
      },
    };
  },

  bob(rt, p) {
    const phase = Math.random() * Math.PI * 2;
    return {
      update(h, t) {
        rt.motion.offset.y += Math.sin(t * p.speed * Math.PI * 2 + phase) * p.height;
      },
    };
  },

  patrol(rt, p) {
    return {
      update(h, t) {
        if (p.distance <= 0) return;
        const period = (2 * p.distance) / Math.max(0.01, p.speed);
        const u = ((t % period) / period) * Math.PI * 2;
        // Smooth back-and-forth that starts at the original spot.
        const d = (1 - Math.cos(u)) * 0.5 * p.distance;
        rt.motion.offset.addScaledVector(AXES[p.axis] || AXES.x, d);
      },
    };
  },

  chase(rt, p, game) {
    return {
      update(h) {
        const player = game.playerPosition;
        _v.set(player.x - rt.home.x, 0, player.z - rt.home.z);
        const dist = _v.length();
        if (dist > p.range || dist < 0.05 || game.state !== 'playing') {
          if (rt.body && rt.body.type === 1) { rt.body.velocity.x *= 0.9; rt.body.velocity.z *= 0.9; }
          return;
        }
        _v.normalize();
        if (rt.body && rt.body.type === 1) {
          // Dynamic objects are pushed with velocity so they still fall and collide.
          rt.body.velocity.x = _v.x * p.speed;
          rt.body.velocity.z = _v.z * p.speed;
          rt.body.wakeUp();
        } else {
          rt.home.addScaledVector(_v, Math.min(dist, p.speed * h));
        }
        rt.faceYaw = Math.atan2(_v.x, _v.z);
      },
    };
  },

  collectible(rt, p, game) {
    return {
      touch() {
        if (!rt.alive) return;
        game.collect(rt, p);
      },
    };
  },

  hazard(rt, p, game) {
    return { touch: () => game.hurt(p.damage, rt) , stay: () => game.hurt(p.damage, rt) };
  },

  goal(rt, p, game) {
    return { touch: () => game.reachGoal(rt) };
  },

  checkpoint(rt, p, game) {
    return { touch: () => game.setCheckpoint(rt) };
  },

  bounce(rt, p, game) {
    return { touch: () => game.bouncePlayer(p.power) };
  },

  speed(rt, p, game) {
    return { touch: () => game.boostPlayer(p.multiplier, p.seconds) };
  },

  teleport(rt, p, game) {
    return { touch: () => game.teleportPlayer(new THREE.Vector3(...p.target)) };
  },

  vanish(rt, p, game) {
    let timer = -1;
    let back = -1;
    return {
      touch() {
        if (timer < 0 && rt.alive) timer = p.delay;
      },
      update(h) {
        if (timer >= 0) {
          timer -= h;
          // Wobble a little before vanishing so the player sees it coming.
          rt.motion.offset.x += Math.sin(timer * 60) * 0.03;
          if (timer < 0) {
            game.setAlive(rt, false);
            if (p.comeBack > 0) back = p.comeBack;
          }
        } else if (back >= 0) {
          back -= h;
          if (back < 0) game.setAlive(rt, true);
        }
      },
    };
  },

  message(rt, p, game) {
    let shown = false;
    return {
      touch() {
        if (p.once && shown) return;
        shown = true;
        game.message(p.text, 3);
      },
    };
  },

  door(rt, p, game) {
    let lastHint = -10;
    return {
      touch() {
        if (!rt.alive) return;
        const need = (p.needs || '').trim().toLowerCase();
        if (!need || game.inventory.has(need)) {
          game.setAlive(rt, false);
          game.sound('door');
          game.message('The door opened!', 2);
        } else if (game.time - lastHint > 2.5) {
          lastHint = game.time;
          game.message(`This door is locked. Find the ${need}!`, 2.5);
        }
      },
    };
  },

  spawner(rt, p, game) {
    let timer = p.every * 0.5;
    const spawned = [];
    return {
      update(h) {
        if (game.state !== 'playing' || !rt.alive) return;
        timer -= h;
        if (timer > 0) return;
        timer = p.every;
        while (spawned.length >= p.max) game.despawn(spawned.shift());
        const pos = rt.obj.position.clone();
        const bounds = rt.obj.userData.localBounds;
        pos.y += (bounds ? bounds.min.y * rt.obj.scale.y : 0) - p.size * 0.6;
        let velocity = null;
        if (p.throwSpeed > 0) {
          const target = game.playerPosition.clone();
          velocity = target.sub(pos).normalize().multiplyScalar(p.throwSpeed);
          velocity.y += 2;
          pos.addScaledVector(velocity.clone().normalize(), Math.max(0.8, p.size));
        }
        const ball = game.spawn({
          name: 'Ball', shape: 'sphere', position: pos.toArray(), scale: [p.size, p.size, p.size],
          color: p.color, look: 'shiny', physics: 'dynamic', mass: 0.5, bounciness: 0.5,
          behaviors: p.harmful ? [{ type: 'hazard', params: { damage: 1 } }] : [],
        }, { velocity, lifetime: 14 });
        if (ball) spawned.push(ball);
      },
    };
  },

  script(rt, p, game) {
    const api = game.makeScriptApi(rt);
    let hooks = null;
    try {
      // eslint-disable-next-line no-new-func
      const factory = new Function('self', 'game', 'player', 'THREE', 'world', 'log',
        `"use strict";\n${p.code}\n;return {\n onStart: typeof onStart === "function" ? onStart : null,\n onUpdate: typeof onUpdate === "function" ? onUpdate : null,\n onTouch: typeof onTouch === "function" ? onTouch : null };`);
      hooks = factory(api.self, api.game, api.player, THREE, api.world, api.log);
    } catch (err) {
      game.scriptError(rt, err);
    }
    const guard = (fn, ...args) => {
      if (!fn) return;
      try {
        fn(...args);
      } catch (err) {
        game.scriptError(rt, err);
        hooks = null;
      }
    };
    return {
      start: () => guard(hooks?.onStart),
      update: (h) => guard(hooks?.onUpdate, h),
      touch: () => guard(hooks?.onTouch),
    };
  },
};
