import { describe, expect, it } from 'vitest';
import { applyActions, describeSummary, specToEntity } from '../src/ai/actions.js';
import { parseAssistantReply } from '../src/ai/parse.js';
import { offlineAssistant } from '../src/ai/offline.js';
import { buildMessages, systemPrompt, worldSnapshot } from '../src/ai/prompt.js';
import { createEntity, createProject } from '../src/core/project.js';

const base = () => {
  const p = createProject('Test');
  p.objects.push(createEntity('box', { id: 'box_1', name: 'Red Box', color: 'red', position: [2, 0.5, 0] }));
  return p;
};

describe('reading AI replies', () => {
  it('reads plain JSON', () => {
    const r = parseAssistantReply('{"message":"Hi","actions":[{"do":"clear"}]}');
    expect(r).toMatchObject({ message: 'Hi', actions: [{ do: 'clear' }], ok: true });
  });
  it('reads fenced JSON with extra text and trailing commas', () => {
    const r = parseAssistantReply('Sure!\n```json\n{"message": "Done", "actions": [{"do": "add", "object": {"prefab": "coin"},},],}\n```\nEnjoy');
    expect(r.ok).toBe(true);
    expect(r.actions).toHaveLength(1);
  });
  it('handles braces inside strings', () => {
    const r = parseAssistantReply('{"message":"use { and } freely","actions":[]}');
    expect(r.message).toBe('use { and } freely');
  });
  it('falls back to text', () => {
    const r = parseAssistantReply('I cannot do that');
    expect(r).toMatchObject({ ok: false, actions: [], message: 'I cannot do that' });
  });
});

describe('applying actions', () => {
  it('adds a prefab resting on the ground with its rules', () => {
    const { project, summary } = applyActions(base(), [{ do: 'add', object: { prefab: 'coin', color: 'blue' } }]);
    const coin = project.objects.at(-1);
    expect(summary.added).toBe(1);
    expect(coin.color).toBe('#0000ff');
    expect(coin.behaviors.map((b) => b.type)).toEqual(['collectible', 'spin']);
    expect(coin.position[1]).toBeGreaterThan(0.3);
  });

  it('adds extra behaviors on top of a prefab', () => {
    const e = specToEntity({ prefab: 'coin', behaviors: ['bob'] });
    expect(e.behaviors.map((b) => b.type)).toEqual(['collectible', 'spin', 'bob']);
    const r = specToEntity({ prefab: 'coin', behaviors: ['bob'], replaceBehaviors: true });
    expect(r.behaviors.map((b) => b.type)).toEqual(['bob']);
  });

  it('treats a game-piece shape as its prefab', () => {
    const e = specToEntity({ shape: 'enemy' });
    expect(e.behaviors.map((b) => b.type)).toContain('hazard');
  });

  it('lays out many objects', () => {
    const { project } = applyActions(base(), [{ do: 'addMany', object: { prefab: 'coin' }, count: 8, layout: 'circle', radius: 5, center: [0, 1, 0] }]);
    const coins = project.objects.filter((o) => o.shape === 'coin');
    expect(coins).toHaveLength(8);
    for (const c of coins) expect(Math.hypot(c.position[0], c.position[2])).toBeCloseTo(5, 5);
  });

  it('updates by id, name and selection', () => {
    let { project } = applyActions(base(), [{ do: 'update', name: 'red box', set: { color: 'green', scale: 2 } }]);
    const box = project.objects.find((o) => o.id === 'box_1');
    expect(box.color).toBe('#008000');
    expect(box.scale).toEqual([2, 2, 2]);
    ({ project } = applyActions(project, [{ do: 'update', target: 'selected', scaleBy: 0.5, addBehavior: 'spin' }], { selectedId: 'box_1' }));
    const box2 = project.objects.find((o) => o.id === 'box_1');
    expect(box2.scale).toEqual([1, 1, 1]);
    expect(box2.behaviors[0].type).toBe('spin');
  });

  it('deletes, clears and changes the world', () => {
    const { project, summary } = applyActions(base(), [
      { do: 'delete', id: 'box_1' },
      { do: 'environment', set: { preset: 'night', fog: 0.5 } },
      { do: 'settings', set: { camera: 'first-person', lives: 5, bogus: 1 } },
    ]);
    expect(project.objects.find((o) => o.id === 'box_1')).toBeUndefined();
    expect(project.environment.preset).toBe('night');
    expect(project.environment.stars).toBe(true);
    expect(project.environment.fog).toBe(0.5);
    expect(project.settings).toMatchObject({ camera: 'first-person', lives: 5 });
    expect(describeSummary(summary)).toMatch(/Removed 1 object/);
    const cleared = applyActions(project, [{ do: 'add', object: { shape: 'box' } }, { do: 'clear' }]).project;
    expect(cleared.objects.map((o) => o.shape)).toEqual(['spawn']);
  });

  it('keeps only one player start', () => {
    const { project } = applyActions(base(), [{ do: 'add', object: { prefab: 'start', position: [5, 0, 5] } }]);
    expect(project.objects.filter((o) => o.shape === 'spawn')).toHaveLength(1);
  });

  it('does not change the input project and ignores junk', () => {
    const p = base();
    const before = JSON.stringify(p);
    const { warnings } = applyActions(p, [null, 42, { do: 'explode' }, { do: 'update', id: 'missing', set: {} }]);
    expect(JSON.stringify(p)).toBe(before);
    expect(warnings.length).toBe(2);
  });
});

describe('built-in helper', () => {
  const run = (text, ctx) => applyActions(base(), offlineAssistant(text, ctx).actions, ctx).project;

  it('adds counted, colored things', () => {
    const p = run('add 5 red cubes in a line');
    const cubes = p.objects.filter((o) => o.shape === 'box' && o.color === '#ff0000' && o.id !== 'box_1');
    expect(cubes).toHaveLength(5);
  });
  it('changes the sky', () => {
    expect(run('make it night').environment.preset).toBe('night');
  });
  it('edits the selected object', () => {
    const p = run('make it bigger and make it spin', { selectedId: 'box_1' });
    const box = p.objects.find((o) => o.id === 'box_1');
    expect(box.scale[0]).toBeCloseTo(1.5);
    expect(box.behaviors.map((b) => b.type)).toContain('spin');
  });
  it('deletes by kind', () => {
    const p = applyActions(run('add 3 trees'), offlineAssistant('delete all trees').actions).project;
    expect(p.objects.some((o) => o.shape === 'tree')).toBe(false);
  });
  it('builds an obstacle course with a goal', () => {
    const p = run('build me an obstacle course');
    expect(p.objects.some((o) => o.behaviors.some((b) => b.type === 'goal'))).toBe(true);
    expect(p.objects.filter((o) => o.shape === 'coin').length).toBeGreaterThan(5);
  });
  it('builds a maze', () => {
    const p = run('maze please');
    expect(p.objects.filter((o) => o.name.startsWith('Wall')).length).toBeGreaterThan(20);
  });
  it('explains itself when it does not understand', () => {
    const r = offlineAssistant('qwerty zxcv');
    expect(r.actions).toEqual([]);
    expect(r.message).toMatch(/AI settings/);
  });
});

describe('prompt', () => {
  it('describes the engine and the world', () => {
    const sys = systemPrompt();
    expect(sys).toContain('addMany');
    expect(sys).toContain('collectible');
    expect(worldSnapshot(base(), 'box_1')).toContain('box_1 ("Red Box")');
  });
  it('keeps messages alternating and starting with the user', () => {
    const msgs = buildMessages([
      { role: 'assistant', content: 'welcome' },
      { role: 'user', content: 'a' },
      { role: 'user', content: 'b' },
      { role: 'assistant', content: 'ok' },
    ], 'add a coin', base(), null);
    expect(msgs.map((m) => m.role)).toEqual(['user', 'assistant', 'user']);
    expect(msgs.at(-1).content).toContain('Request: add a coin');
  });
});

describe('built-in helper counting', () => {
  it('adds one set of spikes but several coins', () => {
    expect(offlineAssistant('add spikes').actions[0].do).toBe('add');
    expect(offlineAssistant('add coins').actions[0].count).toBe(5);
    expect(offlineAssistant('add a coin').actions[0].do).toBe('add');
  });
});
