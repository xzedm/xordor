// Keyboard, mouse and touch controls for play mode.
export class Input {
  constructor(element, { pointerLock = false } = {}) {
    this.el = element;
    this.keys = new Set();
    this.look = { x: 0, y: 0 };
    this.zoom = 0;
    this.jumpPressed = false;
    this.move = { x: 0, y: 0 }; // from touch joystick
    this.pointerLock = pointerLock;
    this.dragging = false;
    this.touchLookId = null;
    this.joyId = null;
    this.listeners = [];
    this.touchUI = null;

    this.on(window, 'keydown', (e) => {
      if (e.target instanceof HTMLElement && e.target.closest('input, textarea, select')) return;
      const code = e.code;
      if (['Space', 'ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight'].includes(code)) e.preventDefault();
      if (code === 'Space' && !this.keys.has('Space')) this.jumpPressed = true;
      this.keys.add(code);
    });
    this.on(window, 'keyup', (e) => this.keys.delete(e.code));
    this.on(window, 'blur', () => this.keys.clear());

    this.on(element, 'pointerdown', (e) => {
      if (e.pointerType === 'touch') return;
      if (this.pointerLock && document.pointerLockElement !== element) {
        try {
          const p = element.requestPointerLock?.();
          if (p && p.catch) p.catch(() => {});
        } catch { /* pointer lock is optional */ }
      }
      this.dragging = true;
      this.lastX = e.clientX;
      this.lastY = e.clientY;
    });
    this.on(window, 'pointerup', () => { this.dragging = false; });
    this.on(window, 'pointermove', (e) => {
      if (e.pointerType === 'touch') return;
      if (document.pointerLockElement === element) {
        this.look.x += e.movementX;
        this.look.y += e.movementY;
      } else if (this.dragging) {
        this.look.x += e.clientX - this.lastX;
        this.look.y += e.clientY - this.lastY;
        this.lastX = e.clientX;
        this.lastY = e.clientY;
      }
    });
    this.on(element, 'wheel', (e) => { this.zoom += Math.sign(e.deltaY); e.preventDefault(); }, { passive: false });
    this.on(element, 'contextmenu', (e) => e.preventDefault());

    // Touch: drag on the right side of the screen to look around.
    this.on(element, 'touchstart', (e) => {
      for (const t of e.changedTouches) {
        if (this.touchLookId === null) {
          this.touchLookId = t.identifier;
          this.lastX = t.clientX;
          this.lastY = t.clientY;
        }
      }
    }, { passive: true });
    this.on(element, 'touchmove', (e) => {
      for (const t of e.changedTouches) {
        if (t.identifier === this.touchLookId) {
          this.look.x += (t.clientX - this.lastX) * 1.4;
          this.look.y += (t.clientY - this.lastY) * 1.4;
          this.lastX = t.clientX;
          this.lastY = t.clientY;
        }
      }
      e.preventDefault();
    }, { passive: false });
    this.on(element, 'touchend', (e) => {
      for (const t of e.changedTouches) if (t.identifier === this.touchLookId) this.touchLookId = null;
    });
  }

  on(target, type, fn, opts) {
    target.addEventListener(type, fn, opts);
    this.listeners.push([target, type, fn, opts]);
  }

  /** Adds an on-screen joystick and jump button (shown on touch screens). */
  attachTouchControls(container) {
    const wrap = document.createElement('div');
    wrap.className = 'touch-controls';
    wrap.innerHTML = '<div class="joy"><div class="joy-knob"></div></div><button class="jump-btn" type="button" aria-label="Jump">Jump</button>';
    container.appendChild(wrap);
    this.touchUI = wrap;
    const joy = wrap.querySelector('.joy');
    const knob = wrap.querySelector('.joy-knob');
    const jump = wrap.querySelector('.jump-btn');
    const setKnob = (x, y) => { knob.style.transform = `translate(${x * 34}px, ${y * 34}px)`; };
    const handle = (t) => {
      const r = joy.getBoundingClientRect();
      let x = (t.clientX - (r.left + r.width / 2)) / (r.width / 2);
      let y = (t.clientY - (r.top + r.height / 2)) / (r.height / 2);
      const len = Math.hypot(x, y);
      if (len > 1) { x /= len; y /= len; }
      this.move.x = x;
      this.move.y = y;
      setKnob(x, y);
    };
    this.on(joy, 'touchstart', (e) => { e.stopPropagation(); this.joyId = e.changedTouches[0].identifier; handle(e.changedTouches[0]); }, { passive: true });
    this.on(joy, 'touchmove', (e) => {
      e.stopPropagation();
      for (const t of e.changedTouches) if (t.identifier === this.joyId) handle(t);
      e.preventDefault();
    }, { passive: false });
    const end = (e) => {
      for (const t of e.changedTouches) {
        if (t.identifier === this.joyId) { this.joyId = null; this.move.x = this.move.y = 0; setKnob(0, 0); }
      }
    };
    this.on(joy, 'touchend', end);
    this.on(joy, 'touchcancel', end);
    this.on(jump, 'touchstart', (e) => { e.stopPropagation(); e.preventDefault(); this.jumpPressed = true; this.keys.add('TouchJump'); }, { passive: false });
    this.on(jump, 'touchend', () => this.keys.delete('TouchJump'));
    this.on(jump, 'click', () => { this.jumpPressed = true; });
  }

  /** Movement direction from keys and joystick: x = right, y = forward. */
  axis() {
    let x = this.move.x;
    let y = -this.move.y;
    const k = this.keys;
    if (k.has('KeyW') || k.has('ArrowUp')) y += 1;
    if (k.has('KeyS') || k.has('ArrowDown')) y -= 1;
    if (k.has('KeyD') || k.has('ArrowRight')) x += 1;
    if (k.has('KeyA') || k.has('ArrowLeft')) x -= 1;
    const len = Math.hypot(x, y);
    if (len > 1) { x /= len; y /= len; }
    return { x, y };
  }

  consumeLook() {
    const l = { ...this.look };
    this.look.x = this.look.y = 0;
    return l;
  }

  consumeZoom() {
    const z = this.zoom;
    this.zoom = 0;
    return z;
  }

  consumeJump() {
    const j = this.jumpPressed;
    this.jumpPressed = false;
    return j;
  }

  get running() {
    return this.keys.has('ShiftLeft') || this.keys.has('ShiftRight');
  }

  dispose() {
    for (const [t, type, fn, opts] of this.listeners) t.removeEventListener(type, fn, opts);
    this.listeners = [];
    this.touchUI?.remove();
    if (document.pointerLockElement === this.el) document.exitPointerLock?.();
  }
}
