// Tiny sound effects made with the Web Audio API, so no sound files are needed.
let ctx = null;
let muted = false;

function audio() {
  if (muted) return null;
  try {
    if (!ctx) ctx = new (window.AudioContext || window.webkitAudioContext)();
    if (ctx.state === 'suspended') ctx.resume();
    return ctx;
  } catch {
    return null;
  }
}

function tone(freq, start, dur, { type = 'square', vol = 0.12, slide = 0 } = {}) {
  const a = audio();
  if (!a) return;
  const t = a.currentTime + start;
  const osc = a.createOscillator();
  const gain = a.createGain();
  osc.type = type;
  osc.frequency.setValueAtTime(freq, t);
  if (slide) osc.frequency.exponentialRampToValueAtTime(Math.max(30, freq + slide), t + dur);
  gain.gain.setValueAtTime(vol, t);
  gain.gain.exponentialRampToValueAtTime(0.0001, t + dur);
  osc.connect(gain).connect(a.destination);
  osc.start(t);
  osc.stop(t + dur + 0.02);
}

function noise(dur, vol) {
  const a = audio();
  if (!a) return;
  const len = Math.floor(a.sampleRate * dur);
  const buf = a.createBuffer(1, len, a.sampleRate);
  const data = buf.getChannelData(0);
  for (let i = 0; i < len; i++) data[i] = (Math.random() * 2 - 1) * (1 - i / len) ** 2;
  const src = a.createBufferSource();
  const gain = a.createGain();
  gain.gain.value = vol;
  src.buffer = buf;
  src.connect(gain).connect(a.destination);
  src.start();
}

const SOUNDS = {
  coin: () => { tone(988, 0, 0.08); tone(1319, 0.07, 0.18); },
  jump: () => tone(320, 0, 0.18, { type: 'triangle', vol: 0.15, slide: 380 }),
  hurt: () => { tone(220, 0, 0.25, { type: 'sawtooth', vol: 0.12, slide: -150 }); },
  bounce: () => tone(200, 0, 0.3, { type: 'sine', vol: 0.2, slide: 700 }),
  win: () => [523, 659, 784, 1047].forEach((f, i) => tone(f, i * 0.12, 0.25, { type: 'triangle', vol: 0.14 })),
  lose: () => [392, 330, 262, 196].forEach((f, i) => tone(f, i * 0.16, 0.3, { type: 'triangle', vol: 0.14 })),
  power: () => { tone(440, 0, 0.1, { type: 'square' }); tone(660, 0.08, 0.1); tone(880, 0.16, 0.2); },
  door: () => tone(140, 0, 0.4, { type: 'sawtooth', vol: 0.08, slide: 120 }),
  teleport: () => tone(300, 0, 0.35, { type: 'sine', vol: 0.18, slide: 1200 }),
  click: () => tone(660, 0, 0.05, { type: 'sine', vol: 0.06 }),
  pop: () => tone(500, 0, 0.08, { type: 'sine', vol: 0.1, slide: -200 }),
  shoot: () => tone(880, 0, 0.09, { type: 'square', vol: 0.07, slide: -500 }),
  zap: () => tone(520, 0, 0.12, { type: 'sawtooth', vol: 0.05, slide: -300 }),
  hit: () => tone(260, 0, 0.06, { type: 'square', vol: 0.08, slide: -80 }),
  explode: () => { noise(0.45, 0.25); tone(90, 0, 0.4, { type: 'sawtooth', vol: 0.12, slide: -60 }); },
  car: () => tone(180, 0, 0.25, { type: 'sawtooth', vol: 0.06, slide: 120 }),
};

export function playSound(name) {
  try {
    SOUNDS[name]?.();
  } catch {
    /* sound is optional */
  }
}

export function setMuted(v) {
  muted = !!v;
}

export function isMuted() {
  return muted;
}

export function unlockAudio() {
  audio();
}
