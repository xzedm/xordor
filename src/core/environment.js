// Ready-made skies and moods. Each preset is plain data, so the AI helper and
// the editor can both use them, and a user can tweak any value afterwards.

export const ENV_PRESETS = {
  day: {
    label: 'Sunny day',
    skyTop: '#3d8fe0', skyBottom: '#cfe8ff', sunColor: '#fff4dc', sunIntensity: 2.6,
    ambient: 0.9, fog: 0.15, fogColor: '#cfe8ff', stars: false, sunHeight: 55,
    groundColor: '#6dbb5a', groundPattern: 'grass',
  },
  sunset: {
    label: 'Sunset',
    skyTop: '#4a3a8c', skyBottom: '#ff9a62', sunColor: '#ffb27a', sunIntensity: 2.2,
    ambient: 0.7, fog: 0.3, fogColor: '#f3a27a', stars: false, sunHeight: 12,
    groundColor: '#7aa25a', groundPattern: 'grass',
  },
  night: {
    label: 'Night',
    skyTop: '#05070f', skyBottom: '#1b2550', sunColor: '#9fb4ff', sunIntensity: 0.7,
    ambient: 0.35, fog: 0.25, fogColor: '#141b38', stars: true, sunHeight: 40,
    groundColor: '#2f4a3a', groundPattern: 'grass',
  },
  space: {
    label: 'Outer space',
    skyTop: '#000000', skyBottom: '#0c0920', sunColor: '#ffffff', sunIntensity: 2.4,
    ambient: 0.4, fog: 0, fogColor: '#000000', stars: true, sunHeight: 30,
    groundColor: '#555a66', groundPattern: 'stone',
  },
  foggy: {
    label: 'Foggy',
    skyTop: '#9aa6b2', skyBottom: '#d7dde2', sunColor: '#ffffff', sunIntensity: 1.2,
    ambient: 1.1, fog: 0.75, fogColor: '#c9d0d6', stars: false, sunHeight: 60,
    groundColor: '#7f8f7a', groundPattern: 'grass',
  },
  desert: {
    label: 'Desert',
    skyTop: '#4aa3df', skyBottom: '#f6e3b4', sunColor: '#fff0c9', sunIntensity: 3,
    ambient: 0.9, fog: 0.2, fogColor: '#f2dcaa', stars: false, sunHeight: 70,
    groundColor: '#e3c27f', groundPattern: 'noise',
  },
  snow: {
    label: 'Snowy',
    skyTop: '#9fc3e6', skyBottom: '#eef5fb', sunColor: '#ffffff', sunIntensity: 2,
    ambient: 1.1, fog: 0.35, fogColor: '#e8f0f7', stars: false, sunHeight: 35,
    groundColor: '#f4f8fb', groundPattern: 'noise',
  },
  candy: {
    label: 'Candy land',
    skyTop: '#ff8fd0', skyBottom: '#ffe3f3', sunColor: '#fff2fb', sunIntensity: 2.2,
    ambient: 1.1, fog: 0.2, fogColor: '#ffd6ee', stars: false, sunHeight: 50,
    groundColor: '#9df0c8', groundPattern: 'checker',
  },
  lava: {
    label: 'Volcano',
    skyTop: '#1a0b0b', skyBottom: '#7a2a12', sunColor: '#ff8a4a', sunIntensity: 1.6,
    ambient: 0.5, fog: 0.4, fogColor: '#3a140c', stars: false, sunHeight: 25,
    groundColor: '#3b2b28', groundPattern: 'stone',
  },
};

export const ENV_PRESET_KEYS = Object.keys(ENV_PRESETS);

export function environmentFromPreset(key) {
  const p = ENV_PRESETS[key] || ENV_PRESETS.day;
  const { label, ...values } = p;
  return { preset: ENV_PRESETS[key] ? key : 'day', ...values, showGround: true, groundSize: 80 };
}
