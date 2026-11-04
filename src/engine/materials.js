// Materials and procedural textures. Every pattern is drawn on a small canvas
// in light greys, so the object's color tints it. No image files needed.
import * as THREE from 'three';

const textureCache = new Map();

function canvas(size = 128) {
  const c = document.createElement('canvas');
  c.width = c.height = size;
  return [c, c.getContext('2d')];
}

function rand(seed) {
  let s = seed;
  return () => {
    s = (s * 16807) % 2147483647;
    return (s - 1) / 2147483646;
  };
}

const painters = {
  checker(g, n) {
    g.fillStyle = '#ffffff'; g.fillRect(0, 0, n, n);
    g.fillStyle = '#b4b4b4';
    g.fillRect(0, 0, n / 2, n / 2); g.fillRect(n / 2, n / 2, n / 2, n / 2);
  },
  bricks(g, n) {
    g.fillStyle = '#d9d9d9'; g.fillRect(0, 0, n, n);
    const r = rand(7);
    const rows = 4; const h = n / rows;
    for (let y = 0; y < rows; y++) {
      const off = y % 2 ? n / 4 : 0;
      for (let x = -1; x < 2; x++) {
        const v = 225 + Math.floor(r() * 30);
        g.fillStyle = `rgb(${v},${v},${v})`;
        g.fillRect(x * n / 2 + off + 3, y * h + 3, n / 2 - 6, h - 6);
      }
    }
  },
  wood(g, n) {
    g.fillStyle = '#ececec'; g.fillRect(0, 0, n, n);
    const r = rand(3);
    for (let i = 0; i < 26; i++) {
      const y = r() * n; const v = 190 + Math.floor(r() * 50);
      g.strokeStyle = `rgba(${v},${v},${v},0.9)`; g.lineWidth = 1 + r() * 3;
      g.beginPath(); g.moveTo(0, y);
      g.bezierCurveTo(n / 3, y + (r() - 0.5) * 10, (2 * n) / 3, y + (r() - 0.5) * 10, n, y);
      g.stroke();
    }
  },
  crate(g, n) {
    painters.wood(g, n);
    g.strokeStyle = '#9a9a9a'; g.lineWidth = n * 0.09;
    g.strokeRect(n * 0.045, n * 0.045, n * 0.91, n * 0.91);
    g.beginPath(); g.moveTo(n * 0.1, n * 0.1); g.lineTo(n * 0.9, n * 0.9); g.stroke();
  },
  grass(g, n) {
    g.fillStyle = '#e6e6e6'; g.fillRect(0, 0, n, n);
    const r = rand(11);
    for (let i = 0; i < 700; i++) {
      const v = 200 + Math.floor(r() * 55);
      g.fillStyle = `rgb(${v},${v},${v})`;
      g.fillRect(r() * n, r() * n, 1.5, 3 + r() * 3);
    }
  },
  stone(g, n) {
    g.fillStyle = '#dcdcdc'; g.fillRect(0, 0, n, n);
    const r = rand(5);
    for (let i = 0; i < 40; i++) {
      const v = 185 + Math.floor(r() * 60);
      g.fillStyle = `rgb(${v},${v},${v})`;
      g.beginPath(); g.ellipse(r() * n, r() * n, 6 + r() * 16, 5 + r() * 12, r() * 3, 0, Math.PI * 2); g.fill();
    }
  },
  noise(g, n) {
    const img = g.createImageData(n, n); const r = rand(9);
    for (let i = 0; i < n * n; i++) {
      const v = 200 + Math.floor(r() * 55);
      img.data[i * 4] = img.data[i * 4 + 1] = img.data[i * 4 + 2] = v; img.data[i * 4 + 3] = 255;
    }
    g.putImageData(img, 0, 0);
  },
  stripes(g, n) {
    g.fillStyle = '#ffffff'; g.fillRect(0, 0, n, n);
    g.fillStyle = '#9c9c9c';
    for (let i = -n; i < n * 2; i += n / 2) {
      g.beginPath(); g.moveTo(i, 0); g.lineTo(i + n / 4, 0); g.lineTo(i + n / 4 - n, n); g.lineTo(i - n, n); g.closePath(); g.fill();
    }
  },
  dots(g, n) {
    g.fillStyle = '#ffffff'; g.fillRect(0, 0, n, n);
    g.fillStyle = '#a8a8a8';
    for (const [x, y] of [[0.25, 0.25], [0.75, 0.75]]) { g.beginPath(); g.arc(x * n, y * n, n * 0.13, 0, Math.PI * 2); g.fill(); }
  },
  tiles(g, n) {
    g.fillStyle = '#c9c9c9'; g.fillRect(0, 0, n, n);
    g.fillStyle = '#f2f2f2';
    g.fillRect(3, 3, n / 2 - 6, n / 2 - 6); g.fillRect(n / 2 + 3, 3, n / 2 - 6, n / 2 - 6);
    g.fillRect(3, n / 2 + 3, n / 2 - 6, n / 2 - 6); g.fillRect(n / 2 + 3, n / 2 + 3, n / 2 - 6, n / 2 - 6);
  },
  arrows(g, n) {
    g.fillStyle = '#9a9a9a'; g.fillRect(0, 0, n, n);
    g.fillStyle = '#ffffff';
    for (const y of [0.15, 0.6]) {
      g.beginPath();
      g.moveTo(n * 0.5, n * y); g.lineTo(n * 0.85, n * (y + 0.3)); g.lineTo(n * 0.65, n * (y + 0.3));
      g.lineTo(n * 0.5, n * (y + 0.15)); g.lineTo(n * 0.35, n * (y + 0.3)); g.lineTo(n * 0.15, n * (y + 0.3));
      g.closePath(); g.fill();
    }
  },
};

export function patternTexture(pattern) {
  if (!pattern || pattern === 'none' || !painters[pattern]) return null;
  if (textureCache.has(pattern)) return textureCache.get(pattern);
  const [c, g] = canvas(128);
  painters[pattern](g, 128);
  const tex = new THREE.CanvasTexture(c);
  tex.wrapS = tex.wrapT = THREE.RepeatWrapping;
  tex.colorSpace = THREE.SRGBColorSpace;
  tex.anisotropy = 4;
  textureCache.set(pattern, tex);
  return tex;
}

/** A canvas texture with text, used by signs. */
export function textTexture(text) {
  const [c, g] = canvas(512);
  c.height = 280;
  g.clearRect(0, 0, c.width, c.height);
  g.fillStyle = '#2a2320';
  g.textAlign = 'center';
  g.textBaseline = 'middle';
  const words = String(text || '').split(/\s+/);
  const lines = [];
  let line = '';
  let size = 72;
  g.font = `700 ${size}px "Fredoka", "Trebuchet MS", sans-serif`;
  for (const w of words) {
    const test = line ? `${line} ${w}` : w;
    if (g.measureText(test).width > 470 && line) { lines.push(line); line = w; } else line = test;
  }
  if (line) lines.push(line);
  if (lines.length > 3) { size = 48; g.font = `700 ${size}px "Fredoka", "Trebuchet MS", sans-serif`; }
  const shown = lines.slice(0, 4);
  shown.forEach((l, i) => g.fillText(l, 256, 140 + (i - (shown.length - 1) / 2) * size * 1.1, 490));
  const tex = new THREE.CanvasTexture(c);
  tex.colorSpace = THREE.SRGBColorSpace;
  return tex;
}

const materialCache = new Map();

/**
 * Returns a (cached) material.
 * opts: { color, look, pattern, repeat, flat, dim, double }
 */
export function getMaterial(opts) {
  const color = opts.color || '#cccccc';
  const look = opts.look || 'plastic';
  const pattern = opts.pattern || 'none';
  const repeat = Math.max(1, Math.min(64, Math.round(opts.repeat || 1)));
  const key = [color, look, pattern, repeat, opts.flat ? 1 : 0, opts.dim || 1, opts.double ? 1 : 0].join('|');
  if (materialCache.has(key)) return materialCache.get(key);

  const c = new THREE.Color(color);
  if (opts.dim && opts.dim !== 1) c.multiplyScalar(opts.dim);
  let map = patternTexture(pattern);
  if (map && repeat !== 1) {
    map = map.clone();
    map.needsUpdate = true;
    map.repeat.set(repeat, repeat);
  }
  let m;
  if (look === 'cartoon') {
    m = new THREE.MeshToonMaterial({ color: c, map });
  } else {
    const params = { color: c, map, flatShading: !!opts.flat };
    switch (look) {
      case 'matte': Object.assign(params, { roughness: 0.95, metalness: 0 }); break;
      case 'shiny': Object.assign(params, { roughness: 0.12, metalness: 0.05 }); break;
      case 'metal': Object.assign(params, { roughness: 0.28, metalness: 0.85 }); break;
      case 'glass': Object.assign(params, { roughness: 0.05, metalness: 0.1, transparent: true, opacity: 0.4, depthWrite: false }); break;
      case 'glow': Object.assign(params, { roughness: 0.4, metalness: 0, emissive: c.clone(), emissiveIntensity: 1.1, emissiveMap: map }); break;
      default: Object.assign(params, { roughness: 0.5, metalness: 0 });
    }
    m = new THREE.MeshStandardMaterial(params);
  }
  if (opts.double || look === 'glass') m.side = THREE.DoubleSide;
  materialCache.set(key, m);
  return m;
}

/** Makes the `mat()` helper that shape builders use. */
export function materialFactory(entity) {
  const s = entity.scale || [1, 1, 1];
  const repeat = Math.max(s[0], s[2], s[1] * 0.75);
  return (overrides = {}) => {
    const fixed = overrides.color !== undefined;
    return getMaterial({
      color: overrides.color ?? entity.color,
      look: overrides.look ?? (fixed ? 'plastic' : entity.look),
      pattern: overrides.pattern ?? (fixed ? 'none' : entity.pattern),
      repeat: overrides.pattern || (!fixed && entity.pattern !== 'none') ? repeat : 1,
      flat: overrides.flat,
      dim: overrides.dim,
      double: overrides.double,
    });
  };
}
