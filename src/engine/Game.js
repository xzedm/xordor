// Play mode: runs a project as a real game with physics, a player, rules and a HUD.
import * as THREE from 'three';
import { CANNON, createBody, createPhysicsWorld } from './physics.js';
import { WorldView } from './world.js';
import { Input } from './input.js';
import { RUNTIME } from './behaviorRuntime.js';
import { playSound, unlockAudio } from './audio.js';
import { MOTION_BEHAVIORS } from '../core/behaviors.js';
import { SHAPES } from '../core/shapes.js';
import { createEntity } from '../core/project.js';
import { deepClone } from '../core/util.js';

const VISUAL_HOOKS = new Set(['spin', 'bob', 'patrol']);

const STEP = 1 / 60;
const PLAYER_RADIUS = 0.45;
const UP = new CANNON.Vec3(0, 1, 0);
const _v = new THREE.Vector3();
const _v2 = new THREE.Vector3();
const _q = new THREE.Quaternion();
const _inv = new THREE.Matrix4();

const ICONS = {
  coin: '<svg viewBox="0 0 24 24" aria-hidden="true"><circle cx="12" cy="12" r="9" fill="#ffc531" stroke="#9a6b00" stroke-width="2"/><circle cx="12" cy="12" r="4.5" fill="none" stroke="#9a6b00" stroke-width="2"/></svg>',
  heart: '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M12 21s-7.5-4.6-9.5-9.2C1 8.3 3.2 4.5 6.9 4.5c2.1 0 3.6 1.2 5.1 3 1.5-1.8 3-3 5.1-3 3.7 0 5.9 3.8 4.4 7.3C19.5 16.4 12 21 12 21z" fill="#ff4d6d"/></svg>',
  clock: '<svg viewBox="0 0 24 24" aria-hidden="true"><circle cx="12" cy="12" r="9" fill="none" stroke="currentColor" stroke-width="2.2"/><path d="M12 7v5l3 2" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round"/></svg>',
  key: '<svg viewBox="0 0 24 24" aria-hidden="true"><circle cx="8" cy="12" r="4" fill="none" stroke="#ffcf33" stroke-width="2.4"/><path d="M12 12h9M18 12v3M21 12v2" stroke="#ffcf33" stroke-width="2.4" stroke-linecap="round"/></svg>',
};

export class Game {
  /**
   * @param {object} opts
   * @param {HTMLElement} opts.container  element that holds the canvas (HUD goes in here)
   * @param {HTMLCanvasElement} opts.canvas
   * @param {object} opts.project         normalized project data
   * @param {() => void} [opts.onExit]    called when the player wants to go back to the editor
   * @param {boolean} [opts.playOnly]     true for published games
   */
  constructor({ container, canvas, project, onExit, playOnly = false }) {
    this.container = container;
    this.canvas = canvas;
    this.project = deepClone(project);
    this.settings = this.project.settings;
    this.onExit = onExit;
    this.playOnly = playOnly;
    this.time = 0;
    this.accumulator = 0;
    this.state = 'ready';
    this.score = 0;
    this.lives = this.settings.lives;
    this.inventory = new Set();
    this.runtime = [];
    this.byId = new Map();
    this.messages = [];
    this.invulnerable = 0;
    this.speedMul = 1;
    this.speedTimer = 0;
    this.jumpBuffer = 0;
    this.coyote = 0;
    this.grounded = false;
    this.groundBody = null;
    this.reportedErrors = new Set();
    this.playerPosition = new THREE.Vector3();
    this.cameraBlockers = [];
    this.raycaster = new THREE.Raycaster();

    this.view = new WorldView({ mode: 'play' });
    this.view.sync(this.project);
    this.scene = this.view.scene;
    this.camera = new THREE.PerspectiveCamera(65, 1, 0.1, 2000);
    this.yaw = 0;
    this.pitch = 0.35;
    this.distance = 7;

    this.world = createPhysicsWorld(this.settings.gravity);
    this.materials = new Map();
    this.playerMaterial = new CANNON.Material({ friction: 0, restitution: 0 });

    this.buildGround();
    for (const e of this.project.objects) this.addRuntime(e, this.view.objects.get(e.id));
    this.buildPlayer();
    this.computeGoals();
    this.buildHud();

    this.input = new Input(canvas, { pointerLock: this.settings.camera === 'first-person' });
    if (matchMedia('(pointer: coarse)').matches || 'ontouchstart' in window) this.input.attachTouchControls(this.hudRoot);
    this.onKey = (e) => {
      if (e.code === 'Escape' && !this.playOnly && this.onExit) this.onExit();
      if (e.code === 'KeyR' && (this.state === 'won' || this.state === 'lost')) this.restart();
      if ((e.code === 'Enter' || e.code === 'Space') && this.state === 'ready') this.begin();
    };
    window.addEventListener('keydown', this.onKey);
    for (const rt of this.runtime) for (const b of rt.hooks) b.start?.();
    this.updateCamera(0, true);
  }

  materialFor(bounciness) {
    const key = Math.round(bounciness * 20);
    if (!this.materials.has(key)) this.materials.set(key, new CANNON.Material({ friction: 0.5, restitution: key / 20 }));
    return this.materials.get(key);
  }

  buildGround() {
    const env = this.project.environment;
    if (!env.showGround) return;
    const half = env.groundSize / 2;
    const body = new CANNON.Body({ type: CANNON.Body.STATIC, material: this.materialFor(0.3) });
    body.addShape(new CANNON.Box(new CANNON.Vec3(half, 1, half)), new CANNON.Vec3(0, -1, 0));
    this.world.addBody(body);
    this.cameraBlockers.push(this.view.ground);
  }

  addRuntime(entity, obj, { velocity = null, lifetime = 0 } = {}) {
    if (!obj) return null;
    const def = SHAPES[entity.shape];
    const motion = entity.behaviors.some((b) => MOTION_BEHAVIORS.has(b.type) && !(b.type === 'chase' && entity.physics === 'dynamic'));
    let bodyType = null;
    if (entity.physics !== 'none' && !def?.editorOnly && entity.shape !== 'light' && entity.visible) {
      bodyType = entity.physics === 'dynamic' && !motion ? 'dynamic' : motion ? 'kinematic' : 'static';
    }
    const rt = {
      entity,
      obj,
      body: null,
      alive: entity.visible,
      moving: motion,
      home: obj.position.clone(),
      homeEuler: obj.rotation.clone(),
      motion: { offset: new THREE.Vector3(), spin: new THREE.Quaternion() },
      faceYaw: null,
      hooks: [],
      touchHooks: [],
      stayHooks: [],
      touching: false,
      lifetime,
    };
    if (bodyType) {
      rt.body = createBody(entity, obj, bodyType, this.materialFor(entity.bounciness));
      if (velocity) rt.body.velocity.set(velocity.x, velocity.y, velocity.z);
      this.world.addBody(rt.body);
      if (bodyType !== 'dynamic') obj.traverse((o) => { if (o.isMesh) this.cameraBlockers.push(o); });
    }
    for (const b of entity.behaviors) {
      const factory = RUNTIME[b.type];
      if (!factory) continue;
      const hook = factory(rt, b.params, this);
      hook.type = b.type;
      rt.hooks.push(hook);
      if (hook.touch) rt.touchHooks.push(hook);
      if (hook.stay) rt.stayHooks.push(hook);
    }
    this.runtime.push(rt);
    this.byId.set(entity.id, rt);
    return rt;
  }

  buildPlayer() {
    const spawn = this.project.objects.find((o) => o.shape === 'spawn');
    this.spawnPoint = spawn ? new THREE.Vector3(spawn.position[0], spawn.position[1] + 0.7, spawn.position[2]) : new THREE.Vector3(0, 1.2, 0);
    this.yaw = spawn ? spawn.rotation[1] * (Math.PI / 180) : 0;
    this.respawnPoint = this.spawnPoint.clone();

    const body = new CANNON.Body({ mass: 1, material: this.playerMaterial, fixedRotation: true, linearDamping: 0, allowSleep: false });
    body.addShape(new CANNON.Sphere(PLAYER_RADIUS));
    body.position.set(this.spawnPoint.x, this.spawnPoint.y, this.spawnPoint.z);
    body.updateMassProperties();
    this.world.addBody(body);
    this.playerBody = body;

    const color = new THREE.Color(this.settings.playerColor);
    const mat = new THREE.MeshStandardMaterial({ color, roughness: 0.45 });
    const g = new THREE.Group();
    const torso = new THREE.Mesh(new THREE.CapsuleGeometry(0.36, 0.58, 8, 20), mat);
    torso.castShadow = true;
    g.add(torso);
    const white = new THREE.MeshStandardMaterial({ color: 0xffffff, roughness: 0.3 });
    const black = new THREE.MeshStandardMaterial({ color: 0x111111, roughness: 0.3 });
    for (const x of [-0.13, 0.13]) {
      const eye = new THREE.Mesh(new THREE.SphereGeometry(0.09, 14, 10), white);
      eye.position.set(x, 0.3, 0.3);
      const pupil = new THREE.Mesh(new THREE.SphereGeometry(0.045, 10, 8), black);
      pupil.position.set(x, 0.3, 0.38);
      g.add(eye, pupil);
    }
    const shadowBlob = new THREE.Mesh(new THREE.CircleGeometry(0.4, 24), new THREE.MeshBasicMaterial({ color: 0x000000, transparent: true, opacity: 0.25, depthWrite: false }));
    shadowBlob.rotation.x = -Math.PI / 2;
    this.shadowBlob = shadowBlob;
    this.scene.add(shadowBlob);
    this.playerMesh = g;
    this.playerMat = mat;
    this.scene.add(g);
    this.playerPosition.copy(this.spawnPoint);
    this.facing = this.yaw + Math.PI;
  }

  computeGoals() {
    const hasGoal = this.runtime.some((rt) => rt.entity.behaviors.some((b) => b.type === 'goal'));
    this.collectTotal = this.runtime.filter((rt) => rt.entity.behaviors.some((b) => b.type === 'collectible' && b.params.points > 0)).length;
    this.collected = 0;
    let cond = this.settings.winCondition;
    if (cond === 'goal' && !hasGoal) cond = this.collectTotal > 0 ? 'collect-all' : 'none';
    if (cond === 'collect-all' && this.collectTotal === 0) cond = hasGoal ? 'goal' : 'none';
    this.winCondition = cond;
    this.timeLeft = this.settings.timeLimit > 0 ? this.settings.timeLimit : cond === 'survive' ? 60 : 0;
  }

  objectiveText() {
    switch (this.winCondition) {
      case 'goal': return 'Reach the finish flag!';
      case 'collect-all': return `Collect all ${this.collectTotal} items!`;
      case 'score': return `Score ${this.settings.targetScore} points!`;
      case 'survive': return `Survive for ${Math.round(this.timeLeft)} seconds!`;
      default: return 'Explore and have fun!';
    }
  }

  // ---------- HUD ----------
  buildHud() {
    const root = document.createElement('div');
    root.className = 'game-hud';
    root.innerHTML = `
      <div class="hud-top">
        <div class="hud-stats">
          <div class="hud-pill" data-score hidden>${ICONS.coin}<span></span></div>
          <div class="hud-pill" data-lives hidden><span></span></div>
          <div class="hud-pill" data-time hidden>${ICONS.clock}<span></span></div>
          <div class="hud-pill" data-items hidden>${ICONS.key}<span></span></div>
        </div>
        ${this.playOnly ? '' : '<button class="hud-stop" type="button" data-stop>Stop <kbd>Esc</kbd></button>'}
      </div>
      <div class="hud-messages" aria-live="polite"></div>
      <div class="hud-overlay" data-overlay></div>
    `;
    this.container.appendChild(root);
    this.hudRoot = root;
    this.hud = {
      score: root.querySelector('[data-score]'),
      lives: root.querySelector('[data-lives]'),
      time: root.querySelector('[data-time]'),
      items: root.querySelector('[data-items]'),
      messages: root.querySelector('.hud-messages'),
      overlay: root.querySelector('[data-overlay]'),
    };
    root.querySelector('[data-stop]')?.addEventListener('click', () => this.onExit?.());
    this.showStartOverlay();
    this.updateHud();
  }

  showStartOverlay() {
    const cam = this.settings.camera;
    const look = cam === 'first-person' ? 'Click, then move the mouse to look' : cam === 'top-down' ? '' : 'Drag with the mouse to look around';
    this.hud.overlay.hidden = false;
    this.hud.overlay.innerHTML = `
      <div class="hud-card">
        <h2>${escapeHtml(this.project.name)}</h2>
        <p class="hud-objective">${escapeHtml(this.settings.startMessage || this.objectiveText())}</p>
        <ul class="hud-keys">
          <li><kbd>W</kbd><kbd>A</kbd><kbd>S</kbd><kbd>D</kbd> or arrow keys to move</li>
          <li><kbd>Space</kbd> to jump, hold <kbd>Shift</kbd> to run</li>
          ${look ? `<li>${look}</li>` : ''}
          <li class="touch-only">On a phone: left stick to move, Jump button to jump</li>
        </ul>
        <button class="btn btn-play" type="button" data-start>Start</button>
      </div>`;
    this.hud.overlay.querySelector('[data-start]').addEventListener('click', () => this.begin());
  }

  begin() {
    if (this.state !== 'ready') return;
    unlockAudio();
    this.state = 'playing';
    this.hud.overlay.hidden = true;
    this.hud.overlay.innerHTML = '';
    if (this.settings.camera === 'first-person') {
      try {
        const p = this.canvas.requestPointerLock?.();
        if (p && p.catch) p.catch(() => {});
      } catch { /* optional */ }
    }
    this.canvas.focus?.();
  }

  updateHud() {
    const { score, lives, time, items } = this.hud;
    const showScore = this.collectTotal > 0 || this.score !== 0 || this.winCondition === 'score';
    score.hidden = !showScore;
    if (showScore) {
      score.querySelector('span').textContent = this.winCondition === 'collect-all'
        ? `${this.collected} / ${this.collectTotal}`
        : this.winCondition === 'score' ? `${this.score} / ${this.settings.targetScore}` : `${this.score}`;
    }
    lives.hidden = this.settings.lives <= 0;
    if (!lives.hidden) {
      const n = Math.max(0, this.lives);
      lives.querySelector('span').innerHTML = n <= 6 ? ICONS.heart.repeat(n) || '0' : `${ICONS.heart} x ${n}`;
    }
    time.hidden = !(this.timeLeft > 0 || (this.settings.timeLimit > 0));
    if (!time.hidden) {
      const t = Math.max(0, Math.ceil(this.timeLeft));
      time.querySelector('span').textContent = `${Math.floor(t / 60)}:${String(t % 60).padStart(2, '0')}`;
      time.classList.toggle('hud-warn', t <= 10);
    }
    items.hidden = this.inventory.size === 0;
    if (!items.hidden) items.querySelector('span').textContent = [...this.inventory].join(', ');
  }

  message(text, seconds = 2.5) {
    if (!text) return;
    const el = document.createElement('div');
    el.className = 'hud-message';
    el.textContent = String(text).slice(0, 200);
    this.hud.messages.appendChild(el);
    while (this.hud.messages.children.length > 3) this.hud.messages.firstChild.remove();
    setTimeout(() => el.classList.add('out'), seconds * 1000);
    setTimeout(() => el.remove(), seconds * 1000 + 400);
  }

  sound(name) {
    playSound(name);
  }

  endGame(won, title, detail) {
    if (this.state !== 'playing') return;
    this.state = won ? 'won' : 'lost';
    this.sound(won ? 'win' : 'lose');
    if (document.pointerLockElement) document.exitPointerLock?.();
    const t = Math.round(this.time);
    this.hud.overlay.hidden = false;
    this.hud.overlay.innerHTML = `
      <div class="hud-card ${won ? 'won' : 'lost'}">
        <h2>${escapeHtml(title)}</h2>
        <p>${escapeHtml(detail || '')}</p>
        <p class="hud-meta">Time ${Math.floor(t / 60)}:${String(t % 60).padStart(2, '0')}${this.score ? ` · Score ${this.score}` : ''}</p>
        <div class="hud-actions">
          <button class="btn btn-play" type="button" data-again>Play again <kbd>R</kbd></button>
          ${this.playOnly ? '' : '<button class="btn" type="button" data-back>Back to editor</button>'}
        </div>
      </div>`;
    this.hud.overlay.querySelector('[data-again]').addEventListener('click', () => this.restart());
    this.hud.overlay.querySelector('[data-back]')?.addEventListener('click', () => this.onExit?.());
  }

  restart() {
    this.onRestart?.();
  }

  // ---------- rules called by behaviors ----------
  collect(rt, p) {
    this.setAlive(rt, false);
    this.score += p.points;
    if (p.points > 0) this.collected++;
    if (p.lives > 0 && this.settings.lives > 0) {
      this.lives += p.lives;
      this.message(`+${p.lives} life`, 1.5);
      this.sound('power');
    } else {
      this.sound('coin');
    }
    if (p.item) {
      this.inventory.add(p.item.trim().toLowerCase());
      this.message(`You got the ${p.item}!`, 2);
    }
    this.updateHud();
    if (this.winCondition === 'collect-all' && this.collected >= this.collectTotal) this.endGame(true, 'You win!', 'You collected everything.');
    if (this.winCondition === 'score' && this.score >= this.settings.targetScore) this.endGame(true, 'You win!', `You reached ${this.settings.targetScore} points.`);
  }

  hurt(damage = 1, source = null) {
    if (this.state !== 'playing' || this.invulnerable > 0 || damage <= 0) return;
    this.sound('hurt');
    this.invulnerable = 1.5;
    if (this.settings.lives > 0) {
      this.lives -= damage;
      this.updateHud();
      if (this.lives <= 0) {
        this.endGame(false, 'Game over', source ? `${source.entity?.name || 'Something'} got you.` : 'You ran out of lives.');
        return;
      }
    }
    this.respawnPlayer();
  }

  respawnPlayer() {
    const b = this.playerBody;
    b.position.set(this.respawnPoint.x, this.respawnPoint.y, this.respawnPoint.z);
    b.velocity.set(0, 0, 0);
  }

  reachGoal() {
    if (this.state !== 'playing') return;
    const left = this.collectTotal - this.collected;
    if (this.winCondition === 'goal') this.endGame(true, 'You win!', 'You reached the finish.');
    else if (this.winCondition === 'collect-all') {
      if (left <= 0) this.endGame(true, 'You win!', 'You collected everything.');
      else this.throttledMessage('goal', `Collect everything first! ${left} left.`);
    } else if (this.winCondition === 'score') {
      if (this.score >= this.settings.targetScore) this.endGame(true, 'You win!', 'You reached the target score.');
      else this.throttledMessage('goal', `You need ${this.settings.targetScore - this.score} more points.`);
    } else this.throttledMessage('goal', 'You reached the finish!');
  }

  throttledMessage(key, text, gap = 2.5) {
    this.lastMsg = this.lastMsg || {};
    if (this.time - (this.lastMsg[key] ?? -99) < gap) return;
    this.lastMsg[key] = this.time;
    this.message(text, gap);
  }

  setCheckpoint(rt) {
    const p = rt.obj.position;
    const next = new THREE.Vector3(p.x, p.y + 0.5, p.z);
    if (next.distanceTo(this.respawnPoint) < 0.5) return;
    this.respawnPoint.copy(next);
    this.message('Checkpoint!', 1.6);
    this.sound('power');
  }

  bouncePlayer(power) {
    this.playerBody.velocity.y = power;
    this.sound('bounce');
  }

  boostPlayer(mult, seconds) {
    if (this.speedTimer <= 0) this.sound('power');
    this.speedMul = mult;
    this.speedTimer = seconds;
  }

  teleportPlayer(target) {
    if (this.time - (this.lastTeleport ?? -9) < 0.6) return;
    this.lastTeleport = this.time;
    this.playerBody.position.set(target.x, target.y, target.z);
    this.playerBody.velocity.set(0, 0, 0);
    this.sound('teleport');
  }

  setAlive(rt, alive) {
    if (rt.alive === alive) return;
    rt.alive = alive;
    rt.obj.visible = alive;
    rt.touching = false;
    if (rt.body) {
      if (alive) {
        this.world.addBody(rt.body);
        if (rt.body.type === CANNON.Body.DYNAMIC) {
          rt.body.position.set(rt.home.x, rt.home.y, rt.home.z);
          rt.body.velocity.set(0, 0, 0);
        }
      } else this.world.removeBody(rt.body);
    }
  }

  spawn(spec, opts = {}) {
    if (this.runtime.length > 4000) return null;
    const entity = createEntity(spec.shape || 'sphere', spec);
    const obj = this.view.addOrUpdate(entity);
    return this.addRuntime(entity, obj, opts);
  }

  despawn(rt) {
    if (!rt) return;
    if (rt.body) this.world.removeBody(rt.body);
    this.view.removeObject(rt.entity.id);
    rt.alive = false;
    rt.dead = true;
  }

  scriptError(rt, err) {
    const key = rt.entity.id;
    console.warn(`[Xordor] Custom code error on "${rt.entity.name}":`, err);
    if (this.reportedErrors.has(key)) return;
    this.reportedErrors.add(key);
    this.message(`Code problem on "${rt.entity.name}": ${err?.message || err}`, 6);
  }

  makeScriptApi(rt) {
    const game = this;
    const colorOf = () => rt.entity.color;
    const self = {
      get name() { return rt.entity.name; },
      position: rt.home,
      rotation: rt.homeEuler,
      scale: rt.obj.scale,
      data: {},
      get color() { return colorOf(); },
      set color(c) {
        rt.obj.traverse((o) => {
          if (o.isMesh && o.material?.color) {
            o.material = o.material.clone();
            o.material.color.set(c);
          }
        });
      },
      hide: () => game.setAlive(rt, false),
      show: () => game.setAlive(rt, true),
      destroy: () => game.despawn(rt),
      distanceToPlayer: () => rt.obj.position.distanceTo(game.playerPosition),
      get touchingPlayer() { return rt.touching; },
    };
    const api = {
      get time() { return game.time; },
      get score() { return game.score; },
      addScore: (n) => { game.score += Number(n) || 0; game.updateHud(); },
      get lives() { return game.lives; },
      win: (msg) => game.endGame(true, 'You win!', msg || ''),
      lose: (msg) => game.endGame(false, 'Game over', msg || ''),
      message: (text, seconds) => game.message(text, seconds || 2.5),
      sound: (name) => game.sound(name),
      spawn: (spec) => {
        const r = game.spawn({ ...spec, position: spec?.position || rt.obj.position.toArray() }, { lifetime: spec?.lifetime || 0 });
        return r ? game.makeScriptApi(r).self : null;
      },
      find: (name) => {
        const r = game.runtime.find((x) => !x.dead && x.entity.name === name);
        return r ? game.makeScriptApi(r).self : null;
      },
      hasItem: (item) => game.inventory.has(String(item).toLowerCase()),
      giveItem: (item) => { game.inventory.add(String(item).toLowerCase()); game.updateHud(); },
    };
    const player = {
      get position() { return game.playerPosition.clone(); },
      get grounded() { return game.grounded; },
      jump: (power) => { game.playerBody.velocity.y = power || game.settings.jumpPower; },
      hurt: (n) => game.hurt(n || 1, rt),
      teleport: (x, y, z) => game.teleportPlayer(new THREE.Vector3(x, y, z)),
    };
    return { self, game: api, player };
  }

  // ---------- simulation ----------
  frame(dt) {
    dt = Math.min(dt, 0.1);
    if (this.state === 'playing') {
      this.accumulator += dt;
      let steps = 0;
      while (this.accumulator >= STEP && steps < 6) {
        this.step(STEP);
        this.accumulator -= STEP;
        steps++;
      }
      if (steps === 6) this.accumulator = 0;
    } else {
      // Keep coins spinning behind the menus.
      this.menuTime = (this.menuTime || 0) + dt;
      this.applyMotion(dt, this.menuTime, true);
    }
    this.syncVisuals();
    this.updateCamera(dt);
    this.view.updateSun(this.playerPosition);
  }

  step(h) {
    this.time += h;
    if (this.invulnerable > 0) this.invulnerable -= h;
    if (this.speedTimer > 0) {
      this.speedTimer -= h;
      if (this.speedTimer <= 0) this.speedMul = 1;
    }
    if (this.timeLeft > 0) {
      const before = Math.ceil(this.timeLeft);
      this.timeLeft -= h;
      if (Math.ceil(this.timeLeft) !== before) this.updateHud();
      if (this.timeLeft <= 0) {
        this.timeLeft = 0;
        if (this.winCondition === 'survive') this.endGame(true, 'You survived!', 'You made it to the end of the timer.');
        else this.endGame(false, "Time's up!", 'Try to be a little faster.');
        return;
      }
    }
    this.applyMotion(h, this.time, false);
    this.movePlayer(h);
    this.world.step(h);
    this.detectGround();
    this.checkTouches();
    for (const rt of this.runtime) {
      if (rt.lifetime > 0) {
        rt.lifetime -= h;
        if (rt.lifetime <= 0) this.despawn(rt);
      }
    }
    if (this.runtime.length > 200 && this.runtime.some((r) => r.dead)) this.runtime = this.runtime.filter((r) => !r.dead);
    const p = this.playerBody.position;
    if (p.y < this.settings.fallLimit) {
      // hurt() respawns the player, unless they are out of lives.
      if (this.settings.lives > 0 && this.invulnerable <= 0) this.hurt(1);
      else this.respawnPlayer();
    }
  }

  applyMotion(h, t, visualOnly) {
    for (const rt of this.runtime) {
      if (rt.dead) continue;
      for (const hook of rt.hooks) if (!visualOnly || VISUAL_HOOKS.has(hook.type)) hook.update?.(h, t);
      if (!rt.moving) continue;
      if (visualOnly && rt.body) {
        rt.motion.offset.set(0, 0, 0);
        rt.motion.spin.identity();
        continue;
      }
      const obj = rt.obj;
      const targetPos = _v.copy(rt.home).add(rt.motion.offset);
      _q.setFromEuler(rt.homeEuler);
      if (rt.faceYaw !== null) _q.setFromAxisAngle(new THREE.Vector3(0, 1, 0), rt.faceYaw).multiply(new THREE.Quaternion().setFromEuler(rt.homeEuler));
      const targetQuat = _q.multiply(rt.motion.spin);
      if (rt.body && rt.body.type === CANNON.Body.KINEMATIC) {
        const b = rt.body;
        b.velocity.set((targetPos.x - b.position.x) / h, (targetPos.y - b.position.y) / h, (targetPos.z - b.position.z) / h);
        const cur = new THREE.Quaternion(b.quaternion.x, b.quaternion.y, b.quaternion.z, b.quaternion.w);
        const delta = targetQuat.clone().multiply(cur.invert());
        if (delta.w < 0) { delta.x *= -1; delta.y *= -1; delta.z *= -1; delta.w *= -1; }
        const angle = 2 * Math.acos(Math.min(1, delta.w));
        const s = Math.sqrt(1 - delta.w * delta.w);
        if (s > 1e-6 && angle > 1e-6) b.angularVelocity.set((delta.x / s) * angle / h, (delta.y / s) * angle / h, (delta.z / s) * angle / h);
        else b.angularVelocity.set(0, 0, 0);
      } else {
        obj.position.copy(targetPos);
        obj.quaternion.copy(targetQuat);
      }
      rt.motion.offset.set(0, 0, 0);
      rt.motion.spin.identity();
    }
  }

  movePlayer(h) {
    const body = this.playerBody;
    const axis = this.input.axis();
    let fx, fz, rx, rz;
    if (this.settings.camera === 'top-down') {
      fx = 0; fz = -1; rx = 1; rz = 0;
    } else {
      fx = -Math.sin(this.yaw); fz = -Math.cos(this.yaw);
      rx = Math.cos(this.yaw); rz = -Math.sin(this.yaw);
    }
    const speed = this.settings.playerSpeed * this.speedMul * (this.input.running ? 1.6 : 1);
    let tx = (fx * axis.y + rx * axis.x) * speed;
    let tz = (fz * axis.y + rz * axis.x) * speed;
    if (this.groundBody && this.groundBody.type === CANNON.Body.KINEMATIC) {
      tx += this.groundBody.velocity.x;
      tz += this.groundBody.velocity.z;
    }
    const accel = this.grounded ? 14 : 5;
    const k = Math.min(1, accel * h);
    body.velocity.x += (tx - body.velocity.x) * k;
    body.velocity.z += (tz - body.velocity.z) * k;
    if (Math.abs(axis.x) + Math.abs(axis.y) > 0.05) {
      const target = Math.atan2(fx * axis.y + rx * axis.x, fz * axis.y + rz * axis.x);
      let d = target - this.facing;
      d = Math.atan2(Math.sin(d), Math.cos(d));
      this.facing += d * Math.min(1, 12 * h);
    }
    if (this.input.consumeJump()) this.jumpBuffer = 0.15;
    if (this.jumpBuffer > 0) this.jumpBuffer -= h;
    if (this.coyote > 0) this.coyote -= h;
    if (this.jumpBuffer > 0 && (this.grounded || this.coyote > 0) && this.settings.jumpPower > 0) {
      body.velocity.y = this.settings.jumpPower;
      this.jumpBuffer = 0;
      this.coyote = 0;
      this.grounded = false;
      this.sound('jump');
    }
  }

  detectGround() {
    const body = this.playerBody;
    let grounded = false;
    let ground = null;
    for (const c of this.world.contacts) {
      let n = null;
      if (c.bi === body) n = c.ni.negate(new CANNON.Vec3());
      else if (c.bj === body) n = c.ni;
      if (n && n.dot(UP) > 0.5) {
        grounded = true;
        ground = c.bi === body ? c.bj : c.bi;
      }
    }
    if (grounded) this.coyote = 0.12;
    this.grounded = grounded;
    this.groundBody = ground;
  }

  checkTouches() {
    const pp = this.playerBody.position;
    this.playerPosition.set(pp.x, pp.y, pp.z);
    const r = PLAYER_RADIUS + 0.06;
    for (const rt of this.runtime) {
      if (rt.dead || (!rt.touchHooks.length && !rt.stayHooks.length)) continue;
      if (!rt.alive) { rt.touching = false; continue; }
      const obj = rt.obj;
      const bounds = obj.userData.localBounds;
      if (!bounds) continue;
      obj.updateMatrixWorld();
      // Quick reject using distance.
      const reach = bounds.max.distanceTo(bounds.min) * 0.5 * Math.max(obj.scale.x, obj.scale.y, obj.scale.z) + r + 1;
      if (obj.position.distanceToSquared(this.playerPosition) > reach * reach) { rt.touching = false; continue; }
      _inv.copy(obj.matrixWorld).invert();
      const local = _v2.copy(this.playerPosition).applyMatrix4(_inv);
      local.clamp(bounds.min, bounds.max).applyMatrix4(obj.matrixWorld);
      const touching = local.distanceTo(this.playerPosition) <= r;
      if (touching && !rt.touching) for (const hook of rt.touchHooks) hook.touch();
      else if (touching) for (const hook of rt.stayHooks) hook.stay();
      rt.touching = touching;
      if (this.state !== 'playing') break;
    }
  }

  syncVisuals() {
    for (const rt of this.runtime) {
      if (rt.dead || !rt.body) continue;
      if (rt.body.type === CANNON.Body.STATIC) continue;
      const b = rt.body;
      rt.obj.position.set(b.position.x, b.position.y, b.position.z);
      rt.obj.quaternion.set(b.quaternion.x, b.quaternion.y, b.quaternion.z, b.quaternion.w);
      if (b.type === CANNON.Body.DYNAMIC) rt.home.copy(rt.obj.position);
    }
    const p = this.playerBody.position;
    this.playerPosition.set(p.x, p.y, p.z);
    this.playerMesh.position.set(p.x, p.y - PLAYER_RADIUS + 0.65, p.z);
    this.playerMesh.rotation.y = this.facing;
    this.playerMesh.visible = this.settings.camera !== 'first-person' && !(this.invulnerable > 0 && Math.floor(this.time * 12) % 2 === 0);
    this.shadowBlob.visible = this.settings.camera !== 'first-person';
    this.shadowBlob.position.set(p.x, p.y - PLAYER_RADIUS + 0.02, p.z);
    this.shadowBlob.material.opacity = this.grounded ? 0.28 : 0.12;
  }

  updateCamera(dt, snap = false) {
    const look = this.input ? this.input.consumeLook() : { x: 0, y: 0 };
    const zoom = this.input ? this.input.consumeZoom() : 0;
    const mode = this.settings.camera;
    const keys = this.input?.keys;
    if (keys?.has('KeyQ')) this.yaw += 2.2 * dt;
    if (keys?.has('KeyE')) this.yaw -= 2.2 * dt;
    const target = _v.copy(this.playerPosition);
    if (mode === 'first-person') {
      this.yaw -= look.x * 0.0025;
      this.pitch = Math.max(-1.45, Math.min(1.45, (this.fpPitch ?? 0) - look.y * 0.0025));
      this.fpPitch = this.pitch;
      this.camera.position.set(target.x, target.y + 0.55, target.z);
      this.camera.rotation.set(this.fpPitch, this.yaw, 0, 'YXZ');
      return;
    }
    if (mode === 'top-down') {
      this.distance = Math.max(8, Math.min(40, this.distance + zoom * 1.2));
      const d = Math.max(this.distance, 14);
      const desired = new THREE.Vector3(target.x, target.y + d, target.z + d * 0.65);
      if (snap) this.camera.position.copy(desired);
      else this.camera.position.lerp(desired, Math.min(1, dt * 6));
      this.camera.lookAt(target);
      return;
    }
    this.yaw -= look.x * 0.005;
    this.pitch = Math.max(-0.25, Math.min(1.3, this.pitch + look.y * 0.005));
    this.distance = Math.max(2.5, Math.min(25, this.distance + zoom * 0.8));
    target.y += 1;
    const dir = new THREE.Vector3(Math.sin(this.yaw) * Math.cos(this.pitch), Math.sin(this.pitch), Math.cos(this.yaw) * Math.cos(this.pitch));
    let dist = this.distance;
    this.raycaster.set(target, dir);
    this.raycaster.far = dist;
    const hits = this.raycaster.intersectObjects(this.cameraBlockers, false);
    for (const hit of hits) {
      if (hit.object.visible && hit.object.parent?.visible !== false) { dist = Math.max(1.2, hit.distance - 0.3); break; }
    }
    const desired = target.clone().addScaledVector(dir, dist);
    if (snap) this.camera.position.copy(desired);
    else this.camera.position.lerp(desired, Math.min(1, dt * 12));
    this.camera.lookAt(target);
  }

  resize(w, h) {
    this.camera.aspect = w / Math.max(1, h);
    this.camera.updateProjectionMatrix();
  }

  render(renderer) {
    renderer.render(this.scene, this.camera);
  }

  dispose() {
    window.removeEventListener('keydown', this.onKey);
    this.input.dispose();
    this.hudRoot.remove();
    this.view.dispose();
  }
}

function escapeHtml(s) {
  return String(s).replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
}

