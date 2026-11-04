// Form fields used by the property panels. Every field shows a plain label
// and calls `onChange(value)` when the person finishes editing.
import { h } from '../ui/dom.js';

let uid = 0;
const nextId = (p) => `${p}-${++uid}`;

export function row(label, control, { hint, id } = {}) {
  return h('div', { class: 'field' },
    h('label', { class: 'field-label', for: id || null }, label),
    control,
    hint ? h('p', { class: 'field-hint' }, hint) : null);
}

export function numberField(label, value, onChange, { min, max, step = 0.1, hint } = {}) {
  const id = nextId('num');
  const input = h('input', { id, type: 'number', class: 'input', value: fmt(value), step, min, max });
  input.addEventListener('change', () => {
    const v = parseFloat(input.value);
    if (Number.isFinite(v)) onChange(v);
  });
  return row(label, input, { hint, id });
}

export function rangeField(label, value, onChange, { min = 0, max = 1, step = 0.01, hint, format = (v) => v } = {}) {
  const id = nextId('rng');
  const out = h('output', { class: 'range-value', for: id }, format(value));
  const input = h('input', { id, type: 'range', class: 'range', value, min, max, step });
  input.addEventListener('input', () => { out.textContent = format(parseFloat(input.value)); });
  input.addEventListener('change', () => onChange(parseFloat(input.value)));
  return row(label, h('div', { class: 'range-wrap' }, input, out), { hint, id });
}

function fmt(v) {
  if (typeof v !== 'number') return v;
  return String(Math.round(v * 1000) / 1000);
}

export function vec3Field(label, value, onChange, { step = 0.1, min, max, key } = {}) {
  const inputs = ['x', 'y', 'z'].map((axis, i) => {
    const input = h('input', { type: 'number', class: `input axis-${axis}`, value: fmt(value[i]), step, min, max, 'aria-label': `${label} ${axis.toUpperCase()}` });
    input.addEventListener('change', () => {
      const vals = inputs.map((el, j) => (Number.isFinite(parseFloat(el.value)) ? parseFloat(el.value) : value[j]));
      onChange(vals);
    });
    return input;
  });
  const wrap = h('div', { class: 'vec3', dataset: key ? { vec: key } : {} },
    ...inputs.map((input, i) => h('label', { class: 'vec3-item' }, h('span', { class: `axis axis-${'xyz'[i]}` }, 'XYZ'[i]), input)));
  wrap.setValues = (vals) => inputs.forEach((el, i) => { if (document.activeElement !== el) el.value = fmt(vals[i]); });
  const r = h('div', { class: 'field' }, h('span', { class: 'field-label' }, label), wrap);
  r.setValues = wrap.setValues;
  return r;
}

export const SWATCHES = ['#ff5a5f', '#ff9f1c', '#ffd23f', '#7ed957', '#2ec4b6', '#3fa7ff', '#5b5bd6', '#b06ce8', '#ff7eb6', '#ffffff', '#9aa3ad', '#2d2f36'];

export function colorField(label, value, onChange, { swatches = true } = {}) {
  const id = nextId('col');
  const input = h('input', { id, type: 'color', class: 'color-input', value });
  input.addEventListener('change', () => onChange(input.value));
  const sw = swatches
    ? h('div', { class: 'swatches' }, SWATCHES.map((c) => h('button', {
      type: 'button', class: `swatch ${c === value ? 'on' : ''}`, style: { background: c }, 'aria-label': `Color ${c}`, title: c,
      onclick: () => onChange(c),
    })))
    : null;
  return row(label, h('div', { class: 'color-wrap' }, input, sw), { id });
}

export function selectField(label, value, options, onChange, { hint } = {}) {
  const id = nextId('sel');
  const select = h('select', { id, class: 'input' },
    Object.entries(options).map(([k, v]) => h('option', { value: k, selected: k === value }, v)));
  select.addEventListener('change', () => onChange(select.value));
  return row(label, select, { hint, id });
}

export function checkField(label, value, onChange, { hint } = {}) {
  const id = nextId('chk');
  const input = h('input', { id, type: 'checkbox', class: 'check', checked: !!value });
  input.addEventListener('change', () => onChange(input.checked));
  return h('div', { class: 'field field-check' }, input, h('label', { for: id }, label), hint ? h('p', { class: 'field-hint' }, hint) : null);
}

export function textField(label, value, onChange, { placeholder = '', hint, multiline = false, code = false } = {}) {
  const id = nextId('txt');
  const input = multiline
    ? h('textarea', { id, class: `input ${code ? 'code' : ''}`, rows: code ? 10 : 3, spellcheck: !code, placeholder }, value)
    : h('input', { id, type: 'text', class: 'input', value, placeholder });
  if (multiline) input.value = value;
  input.addEventListener('change', () => onChange(input.value));
  if (code) {
    input.addEventListener('keydown', (e) => {
      if (e.key === 'Tab') {
        e.preventDefault();
        const s = input.selectionStart;
        input.setRangeText('  ', s, input.selectionEnd, 'end');
      }
    });
  }
  return row(label, input, { hint, id });
}

export function section(title, ...children) {
  return h('section', { class: 'panel-section' }, h('h3', { class: 'section-title' }, title), ...children);
}
