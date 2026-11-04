// Starter games. Each one is built from the same actions the AI helper uses.
import { createProject, levelView, mergeLevelView } from './core/project.js';
import { applyActions } from './ai/actions.js';
import { offlineAssistant } from './ai/offline.js';

export const TEMPLATES = [
  { key: 'empty', label: 'Empty world', blurb: 'A blank grassy field. Build anything.' },
  { key: 'coins', label: 'Coin hunt', blurb: 'Collect every coin before the timer runs out.' },
  { key: 'parkour', label: 'Sky parkour', blurb: 'Jump across floating platforms to the finish flag.' },
  { key: 'arena', label: 'Enemy arena', blurb: 'Dodge the chasers and survive for 60 seconds.' },
  { key: 'playground', label: 'Physics playground', blurb: 'Knock over towers of crates and bounce balls.' },
  { key: 'maze', label: 'Maze escape', blurb: 'Find the way out of a brick maze.' },
];

const recipes = {
  empty: () => [
    { do: 'add', object: { prefab: 'tree', position: [-6, 1.4, -8] } },
    { do: 'add', object: { prefab: 'rock', position: [5, 0.55, -6] } },
    { do: 'add', object: { prefab: 'sign', position: [0, 1.05, -4], props: { text: 'Build something!' } } },
  ],
  coins: () => [
    { do: 'rename', name: 'Coin Hunt' },
    { do: 'settings', set: { winCondition: 'collect-all', timeLimit: 90, lives: 3, startMessage: 'Collect all the coins in 90 seconds!' } },
    { do: 'environment', set: { preset: 'day' } },
    { do: 'addMany', object: { prefab: 'coin' }, count: 12, layout: 'circle', radius: 12, center: [0, 0.6, 0] },
    { do: 'add', object: { prefab: 'platform', position: [0, 1.5, -18], scale: [5, 0.5, 5] } },
    { do: 'add', object: { prefab: 'stairs', position: [0, 0.75, -14.25], scale: [3, 1.5, 2.5] } },
    { do: 'addMany', object: { prefab: 'coin' }, count: 3, layout: 'line', center: [0, 2.7, -18], spacing: 1.5 },
    { do: 'addMany', object: { prefab: 'tree' }, count: 14, layout: 'random', area: [60, 0, 60], seed: 4 },
    { do: 'addMany', object: { prefab: 'rock' }, count: 8, layout: 'random', area: [50, 0, 50], seed: 9 },
    { do: 'add', object: { prefab: 'bouncepad', position: [14, 0.13, 6] } },
    { do: 'add', object: { prefab: 'coin', position: [14, 7, 6] } },
  ],
  parkour: () => [
    { do: 'rename', name: 'Sky Parkour' },
    { do: 'environment', set: { preset: 'sunset', showGround: false } },
    { do: 'settings', set: { winCondition: 'goal', lives: 3, fallLimit: -15, startMessage: 'Jump from platform to platform and reach the flag!' } },
    { do: 'update', shape: 'spawn', set: { position: [0, 0.31, 0] } },
    { do: 'add', object: { prefab: 'platform', name: 'Start platform', position: [0, 0, 0], scale: [6, 0.5, 6] } },
    { do: 'addMany', object: { prefab: 'platform', scale: [3, 0.5, 3], color: '#7fb3d5' }, count: 5, layout: 'stairs', center: [0, 0.4, -6], spacing: 4.5, rise: 0.6, direction: [0, 0, -1] },
    { do: 'add', object: { prefab: 'checkpoint', position: [0, 3.65, -24] } },
    { do: 'add', object: { prefab: 'mover', position: [-1, 3.2, -30], behaviors: [{ type: 'patrol', params: { axis: 'x', distance: 5, speed: 1.6 } }] } },
    { do: 'add', object: { prefab: 'crumble', position: [5.5, 3.6, -35] } },
    { do: 'add', object: { prefab: 'crumble', position: [5.5, 4.2, -39.5] } },
    { do: 'add', object: { prefab: 'spinner', position: [5.5, 5.4, -46], scale: [8, 0.4, 0.4] } },
    { do: 'add', object: { prefab: 'platform', position: [5.5, 4.6, -46], scale: [6, 0.5, 6] } },
    { do: 'add', object: { prefab: 'bouncepad', position: [5.5, 5, -48] } },
    { do: 'add', object: { prefab: 'platform', name: 'Finish platform', position: [5.5, 9, -56], scale: [6, 0.5, 6], color: '#ffd166' } },
    { do: 'add', object: { prefab: 'goal', position: [5.5, 10.5, -57] } },
    { do: 'addMany', object: { prefab: 'coin' }, count: 5, layout: 'stairs', center: [0, 1.8, -6], spacing: 4.5, rise: 0.6, direction: [0, 0, -1] },
    { do: 'addMany', object: { prefab: 'cloud' }, count: 10, layout: 'random', center: [0, -6, -30], area: [70, 6, 80], seed: 2 },
  ],
  arena: () => [
    { do: 'rename', name: 'Enemy Arena' },
    { do: 'environment', set: { preset: 'night', groundPattern: 'tiles', groundColor: '#3a4660' } },
    { do: 'settings', set: { winCondition: 'survive', timeLimit: 60, lives: 3, startMessage: 'Survive for 60 seconds. Grab hearts for extra lives!' } },
    { do: 'addMany', object: { prefab: 'wall', scale: [9, 2.5, 0.6], color: '#56627a', pattern: 'bricks' }, count: 8, layout: 'circle', radius: 13, center: [0, 1.25, 0], faceCenter: true },
    { do: 'addMany', object: { prefab: 'enemy', behaviors: [{ type: 'chase', params: { speed: 2.6, range: 40 } }] }, count: 4, layout: 'circle', radius: 9, center: [0, 0.65, 0] },
    { do: 'addMany', object: { prefab: 'heart' }, count: 3, layout: 'circle', radius: 5, center: [0, 0.6, 0] },
    { do: 'addMany', object: { prefab: 'lamp', color: '#9fd3ff' }, count: 4, layout: 'circle', radius: 11, center: [0, 3, 0] },
    { do: 'add', object: { prefab: 'pillar', position: [0, 2, 0], scale: [1.2, 4, 1.2] } },
  ],
  playground: () => [
    { do: 'rename', name: 'Physics Playground' },
    { do: 'environment', set: { preset: 'candy' } },
    { do: 'settings', set: { winCondition: 'none', lives: 0, startMessage: 'Run into things and knock them over!' } },
    { do: 'addMany', object: { prefab: 'crate' }, count: 6, layout: 'tower', center: [-4, 0.5, -8], spacing: 1.01 },
    { do: 'addMany', object: { prefab: 'crate' }, count: 6, layout: 'tower', center: [-2.9, 0.5, -8], spacing: 1.01 },
    { do: 'addMany', object: { prefab: 'barrel' }, count: 9, layout: 'grid', center: [4, 0.55, -8], spacing: 1.2, columns: 3 },
    { do: 'addMany', object: { prefab: 'bouncyball' }, count: 8, layout: 'random', center: [0, 4, -2], area: [14, 4, 8], seed: 5 },
    { do: 'add', object: { prefab: 'ramp', position: [0, 0.75, 6], rotation: [0, 180, 0] } },
    { do: 'add', object: { prefab: 'dropper', position: [8, 8, 0], behaviors: [{ type: 'spawner', params: { every: 1, harmful: false, color: '#7c4dff' } }] } },
    { do: 'add', object: { prefab: 'speedpad', position: [0, 0.04, 2] } },
  ],
  maze: () => [
    { do: 'rename', name: 'Maze Escape' },
    { do: 'environment', set: { preset: 'foggy' } },
    ...offlineAssistant('build a maze', { focus: [0, 0, 0] }).actions,
  ],
};

/** Builds a starter project (with one level) from a template key. */
export function buildTemplate(key) {
  const recipe = recipes[key] || recipes.empty;
  const base = createProject(TEMPLATES.find((t) => t.key === key)?.label || 'My Game');
  const view = applyActions(levelView(base), recipe()).project;
  return mergeLevelView(base, view);
}

/** Fills an existing level view with a template's contents (used for new levels). */
export function templateActions(key) {
  return (recipes[key] || recipes.empty)().filter((a) => a.do !== 'rename');
}
