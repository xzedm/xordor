import { describe, expect, it } from 'vitest';
import { createEntity, createLevel, createProject, hasScripts, levelView, mergeLevelView, normalizeEntity, normalizeProject, stripScripts } from '../src/core/project.js';
import { parseColor, toVec3 } from '../src/core/util.js';
import { PREFAB_GROUPS, prefabToEntity } from '../src/core/prefabs.js';
import { SHAPES } from '../src/core/shapes.js';
import { BEHAVIORS } from '../src/core/behaviors.js';
import { buildTemplate, TEMPLATES } from '../src/templates.js';

describe('colors', () => {
  it('understands names, short hex and rgb', () => {
    expect(parseColor('red')).toBe('#ff0000');
    expect(parseColor('#0f0')).toBe('#00ff00');
    expect(parseColor('rgb(0, 0, 255)')).toBe('#0000ff');
    expect(parseColor('light blue')).toBe('#add8e6');
    expect(parseColor('not a color', '#123456')).toBe('#123456');
  });
});

describe('vectors', () => {
  it('accepts arrays, numbers and objects', () => {
    expect(toVec3([1, 2, 3])).toEqual([1, 2, 3]);
    expect(toVec3(2)).toEqual([2, 2, 2]);
    expect(toVec3({ x: 1, y: 'a', z: 3 }, [9, 9, 9])).toEqual([1, 9, 3]);
  });
});

describe('project data', () => {
  it('creates a new project with one level and a player start', () => {
    const p = createProject('Test');
    expect(p.name).toBe('Test');
    expect(p.version).toBe(2);
    expect(p.levels).toHaveLength(1);
    expect(p.startLevel).toBe(p.levels[0].id);
    expect(p.levels[0].objects.filter((o) => o.shape === 'spawn')).toHaveLength(1);
  });

  it('upgrades version 1 files to levels', () => {
    const p = normalizeProject({ name: 'Old', environment: { preset: 'night' }, objects: [{ shape: 'coin' }] });
    expect(p.levels).toHaveLength(1);
    expect(p.levels[0].environment.preset).toBe('night');
    expect(p.levels[0].objects[0].shape).toBe('coin');
  });

  it('edits one level through a view without touching the others', () => {
    const p = createProject('Multi');
    p.levels.push(createLevel('Second'));
    const second = p.levels[1].id;
    const view = levelView(p, second);
    expect(view.levelName).toBe('Second');
    expect(view.levels.map((l) => l.name)).toEqual(['Level 1', 'Second']);
    view.objects = [...view.objects, createEntity('box')];
    view.settings = { ...view.settings, lives: 7 };
    const next = mergeLevelView(p, view);
    expect(next.levels[0].objects).toHaveLength(1);
    expect(next.levels[1].objects).toHaveLength(2);
    expect(next.settings.lives).toBe(7);
  });

  it('keeps folders and tags on objects', () => {
    const e = normalizeEntity({ shape: 'box', folder: '/Level/Walls/', tags: 'wall, red ,' });
    expect(e.folder).toBe('Level/Walls');
    expect(e.tags).toEqual(['wall', 'red']);
  });

  it('cleans up bad entity data', () => {
    const e = normalizeEntity({ shape: 'nope', position: [1, 'x'], scale: -5, color: 'blue', physics: 'weird', behaviors: [{ type: 'spin', params: { speed: '45' } }, { type: 'fly' }] });
    expect(e.shape).toBe('box');
    expect(e.position).toEqual([1, 0.5, 0]);
    expect(e.scale.every((v) => v > 0)).toBe(true);
    expect(e.color).toBe('#0000ff');
    expect(e.physics).toBe('static');
    expect(e.behaviors).toEqual([{ type: 'spin', params: { axis: 'y', speed: 45 } }]);
  });

  it('fixes duplicate ids when loading', () => {
    const p = normalizeProject({ objects: [{ id: 'a' }, { id: 'a' }] });
    expect(new Set(p.levels[0].objects.map((o) => o.id)).size).toBe(2);
  });

  it('detects and removes custom code in every level and the game script', () => {
    const p = createProject();
    p.levels.push(createLevel('Two'));
    p.levels[1].objects.push(createEntity('box', { behaviors: [{ type: 'script', params: { code: 'x' } }, { type: 'spin' }] }));
    expect(hasScripts(p)).toBe(true);
    stripScripts(p);
    expect(hasScripts(p)).toBe(false);
    expect(p.levels[1].objects[1].behaviors).toHaveLength(1);
    p.gameScript = 'function onStart() {}';
    expect(hasScripts(p)).toBe(true);
    stripScripts(p);
    expect(p.gameScript).toBe('');
  });

  it('every prefab uses a real shape and real behaviors', () => {
    for (const g of PREFAB_GROUPS) {
      for (const item of g.items) {
        expect(SHAPES[item.shape], item.key).toBeTruthy();
        for (const b of item.behaviors || []) expect(BEHAVIORS[b.type], `${item.key}.${b.type}`).toBeTruthy();
        const { overrides } = prefabToEntity(item.key);
        const e = createEntity(item.shape, overrides);
        expect(e.behaviors.length).toBe((item.behaviors || []).length);
      }
    }
  });

  it('builds every template', () => {
    for (const t of TEMPLATES) {
      const p = buildTemplate(t.key);
      const objects = p.levels[0].objects;
      expect(objects.length, t.key).toBeGreaterThan(0);
      expect(objects.filter((o) => o.shape === 'spawn'), t.key).toHaveLength(1);
    }
  });
});

describe('project files', async () => {
  const { parseProjectFile, projectToFile } = await import('../src/editor/storage.js');
  it('reads back what it saves, levels included', () => {
    const p = createProject('Round trip');
    p.levels.push(createLevel('Two'));
    p.gameScript = 'function onStart() {}';
    const back = parseProjectFile(projectToFile(p));
    expect(back.levels.map((l) => l.name)).toEqual(['Level 1', 'Two']);
    expect(back.gameScript).toBe(p.gameScript);
  });
  it('still reads version 1 files', () => {
    const back = parseProjectFile(JSON.stringify({ format: 'xordor-project', version: 1, name: 'Old', objects: [{ shape: 'coin' }] }));
    expect(back.levels[0].objects[0].shape).toBe('coin');
  });
  it('rejects files that are not projects', () => {
    expect(() => parseProjectFile('{"hello": 1}')).toThrow();
  });
});

describe('weapons and assets', async () => {
  const { normalizeAsset, normalizeSettings } = await import('../src/core/project.js');
  it('keeps only known weapons', () => {
    expect(normalizeSettings({ weapon: 'cannon' }).weapon).toBe('cannon');
    expect(normalizeSettings({ weapon: 'laser sword' }).weapon).toBe('none');
  });
  it('accepts model data URLs and rejects anything else', () => {
    const a = normalizeAsset({ name: 'Robot', data: 'data:model/gltf-binary;base64,Z2xURg==', animations: ['Wave'] });
    expect(a).toMatchObject({ name: 'Robot', kind: 'model', animations: ['Wave'] });
    expect(a.id).toMatch(/^asset_/);
    expect(normalizeAsset({ data: 'https://example.com/robot.glb' })).toBeNull();
    expect(normalizeAsset(null)).toBeNull();
  });
  it('saves models inside the project file', () => {
    const p = normalizeProject({ name: 'With model', assets: [{ id: 'asset_1', name: 'Robot', data: 'data:model/gltf-binary;base64,AAAA' }] });
    expect(p.assets).toHaveLength(1);
  });
});
