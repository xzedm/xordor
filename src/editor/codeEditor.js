// A JavaScript code editor (CodeMirror 6, open source) for game scripts.
import { EditorView, basicSetup } from 'codemirror';
import { javascript } from '@codemirror/lang-javascript';
import { EditorState, StateEffect, StateField } from '@codemirror/state';
import { Decoration, keymap } from '@codemirror/view';
import { indentWithTab } from '@codemirror/commands';
import { HighlightStyle, syntaxHighlighting } from '@codemirror/language';
import { tags as t } from '@lezer/highlight';

// Colors chosen for the dark editor background.
const highlight = HighlightStyle.define([
  { tag: [t.keyword, t.controlKeyword, t.moduleKeyword, t.operatorKeyword], color: '#ff9e64' },
  { tag: [t.definitionKeyword, t.modifier], color: '#ff9e64' },
  { tag: [t.function(t.variableName), t.function(t.propertyName)], color: '#7cc7ff' },
  { tag: [t.definition(t.variableName), t.definition(t.function(t.variableName))], color: '#9fe2a8' },
  { tag: [t.string, t.special(t.string), t.regexp], color: '#c3e88d' },
  { tag: [t.number, t.bool, t.null, t.atom], color: '#ffcb6b' },
  { tag: [t.comment, t.lineComment, t.blockComment], color: '#8a8597', fontStyle: 'italic' },
  { tag: [t.propertyName], color: '#e6d3ff' },
  { tag: [t.self, t.special(t.variableName)], color: '#ff79c6' },
  { tag: [t.operator, t.punctuation, t.bracket], color: '#c9c3d4' },
  { tag: t.invalid, color: '#ff6b6b' },
]);

const setErrorLine = StateEffect.define();
const errorLineField = StateField.define({
  create: () => Decoration.none,
  update(deco, tr) {
    deco = deco.map(tr.changes);
    for (const e of tr.effects) {
      if (e.is(setErrorLine)) {
        if (!e.value) return Decoration.none;
        const line = tr.state.doc.line(Math.min(Math.max(1, e.value), tr.state.doc.lines));
        return Decoration.set([Decoration.line({ class: 'cm-error-line' }).range(line.from)]);
      }
    }
    return deco;
  },
  provide: (f) => EditorView.decorations.from(f),
});

const theme = EditorView.theme({
  '&': { height: '100%', fontSize: '13px', backgroundColor: 'var(--code-bg)', color: 'var(--code-fg)' },
  '.cm-scroller': { fontFamily: 'var(--font-mono)', lineHeight: '1.55' },
  '.cm-gutters': { backgroundColor: 'var(--code-gutter)', color: 'var(--muted)', border: 'none' },
  '.cm-activeLine': { backgroundColor: 'var(--code-active)' },
  '.cm-activeLineGutter': { backgroundColor: 'var(--code-active)' },
  '.cm-cursor': { borderLeftColor: 'var(--accent)' },
  '&.cm-focused .cm-selectionBackground, .cm-selectionBackground': { backgroundColor: 'var(--code-selection) !important' },
  '.cm-error-line': { backgroundColor: 'var(--code-error)' },
}, { dark: true });

/**
 * Creates an editor inside `parent`.
 * opts: { value, onChange(text), onSave(text) }
 */
export function createCodeEditor(parent, { value = '', onChange, onSave } = {}) {
  let silent = false;
  const view = new EditorView({
    parent,
    state: EditorState.create({
      doc: value,
      extensions: [
        basicSetup,
        javascript(),
        theme,
        syntaxHighlighting(highlight),
        errorLineField,
        keymap.of([
          indentWithTab,
          { key: 'Mod-s', preventDefault: true, run: (v) => { onSave?.(v.state.doc.toString()); return true; } },
        ]),
        EditorView.updateListener.of((u) => {
          if (u.docChanged && !silent) onChange?.(u.state.doc.toString());
        }),
      ],
    }),
  });
  return {
    view,
    getValue: () => view.state.doc.toString(),
    setValue(text) {
      if (text === view.state.doc.toString()) return;
      silent = true;
      view.dispatch({ changes: { from: 0, to: view.state.doc.length, insert: text } });
      silent = false;
    },
    markError(line) {
      view.dispatch({ effects: setErrorLine.of(line || null) });
      if (line) {
        const l = view.state.doc.line(Math.min(Math.max(1, line), view.state.doc.lines));
        view.dispatch({ selection: { anchor: l.from }, scrollIntoView: true });
      }
    },
    focus: () => view.focus(),
    destroy: () => view.destroy(),
  };
}
