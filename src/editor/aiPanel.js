// The AI helper chat. It sends your request plus a snapshot of the world to
// the AI you picked, then applies the changes it suggests as one undo step.
import { clear, h, icon, openDialog, toast } from '../ui/dom.js';
import { AIError, PROVIDERS, chat, describeProvider, isReady, listModels } from '../ai/providers.js';
import { buildMessages, systemPrompt } from '../ai/prompt.js';
import { parseAssistantReply } from '../ai/parse.js';
import { applyActions, describeSummary, hasChanges } from '../ai/actions.js';
import { offlineAssistant } from '../ai/offline.js';
import { safeStorage } from '../core/util.js';

const STORE_KEY = 'xordor.ai.v1';
const storage = safeStorage();

const IDEAS = [
  'Build an obstacle course with coins and a finish flag',
  'Make it night time with glowing lights',
  'Add 3 enemies that chase me',
  'Build a castle',
  'Plant a forest',
  'Make the selected object spin and float',
];

export function loadAISettings() {
  try {
    const s = JSON.parse(storage.getItem(STORE_KEY) || '{}');
    return { provider: 'offline', keys: {}, models: {}, baseUrls: {}, effort: 'medium', ...s };
  } catch {
    return { provider: 'offline', keys: {}, models: {}, baseUrls: {}, effort: 'medium' };
  }
}

export function saveAISettings(s) {
  try { storage.setItem(STORE_KEY, JSON.stringify(s)); } catch { /* storage is optional */ }
}

export class AIPanel {
  constructor(root, editor) {
    this.root = root;
    this.editor = editor;
    this.settings = loadAISettings();
    this.history = [];
    this.busy = false;
    this.abort = null;

    this.providerChip = h('button', { type: 'button', class: 'provider-chip', onclick: () => this.openSettings() });
    this.log = h('div', { class: 'chat-log', 'aria-live': 'polite' });
    this.input = h('textarea', { class: 'input chat-input', rows: 2, placeholder: 'Tell the AI what to build or change...', 'aria-label': 'Message to the AI helper' });
    this.sendBtn = h('button', { type: 'button', class: 'btn btn-primary send-btn', 'aria-label': 'Send', onclick: () => (this.busy ? this.cancel() : this.send()) }, icon('send'));
    this.input.addEventListener('keydown', (e) => {
      if (e.key === 'Enter' && !e.shiftKey && !e.isComposing) {
        e.preventDefault();
        this.send();
      }
    });
    this.ideas = h('div', { class: 'ideas' }, IDEAS.map((t) => h('button', { type: 'button', class: 'idea', onclick: () => { this.input.value = t; this.send(); } }, t)));
    root.append(
      h('div', { class: 'chat-head' },
        h('div', { class: 'chat-title' }, icon('sparkle'), h('strong', {}, 'AI helper')),
        this.providerChip),
      this.log,
      h('div', { class: 'chat-compose' }, this.ideas, h('div', { class: 'chat-row' }, this.input, this.sendBtn)));
    this.updateChip();
    this.addBubble('assistant', 'Hi! Tell me what you want in your game, like "make a lava level with coins and a finish flag" or "make this cube bounce". I build it right away, and you can undo anything.');
  }

  updateChip() {
    const d = describeProvider(this.settings);
    clear(this.providerChip);
    this.providerChip.append(...[icon('gear'), h('span', {}, d.label), d.model ? h('span', { class: 'muted' }, ` ${d.model}`) : null].filter(Boolean));
    this.providerChip.title = 'Choose which AI to use';
  }

  focus(prefill = '') {
    if (prefill && !this.input.value) this.input.value = prefill;
    this.input.focus();
    this.input.setSelectionRange(this.input.value.length, this.input.value.length);
  }

  addBubble(role, text, extra = null) {
    const bubble = h('div', { class: `bubble bubble-${role}` }, h('p', {}, text), extra);
    this.log.append(bubble);
    this.log.scrollTop = this.log.scrollHeight;
    return bubble;
  }

  cancel() {
    this.abort?.abort();
  }

  setBusy(on, label = '') {
    this.busy = on;
    clear(this.sendBtn);
    this.sendBtn.append(icon(on ? 'stop' : 'send'));
    this.sendBtn.setAttribute('aria-label', on ? 'Stop' : 'Send');
    this.sendBtn.classList.toggle('busy', on);
    this.input.disabled = on;
    if (!on) this.input.focus();
    if (on) {
      this.thinking = this.addBubble('assistant thinking', label || 'Thinking...');
    } else {
      this.thinking?.remove();
      this.thinking = null;
    }
  }

  async send() {
    const text = this.input.value.trim();
    if (!text || this.busy) return;
    this.input.value = '';
    this.ideas.hidden = true;
    this.addBubble('user', text);
    const ed = this.editor;
    const selected = ed.selected;
    let reply;
    const provider = PROVIDERS[this.settings.provider] || PROVIDERS.offline;

    if (provider.kind === 'offline') {
      reply = offlineAssistant(text, { selectedId: ed.selectedId, selectedName: selected?.name, focus: ed.dropPoint() });
      this.finish(text, reply, { offline: true });
      return;
    }

    const ready = isReady(this.settings);
    if (!ready.ok) {
      this.addBubble('assistant error', ready.reason, h('button', { type: 'button', class: 'btn btn-small', onclick: () => this.openSettings() }, 'Open AI settings'));
      return;
    }

    this.abort = new AbortController();
    const started = performance.now();
    this.setBusy(true, `Asking ${provider.label}...`);
    const tick = setInterval(() => {
      if (this.thinking) this.thinking.querySelector('p').textContent = `Asking ${provider.label}... ${Math.round((performance.now() - started) / 1000)}s`;
    }, 1000);
    try {
      const raw = await chat(this.settings, {
        system: systemPrompt(),
        messages: buildMessages(this.history, text, ed.project, ed.selectedId),
        signal: this.abort.signal,
        onProgress: (chars) => {
          if (this.thinking) this.thinking.querySelector('p').textContent = `Building... (${chars.toLocaleString()} characters so far)`;
        },
      });
      clearInterval(tick);
      this.setBusy(false);
      reply = parseAssistantReply(raw);
      if (!reply.ok && !reply.message) reply.message = 'The AI replied, but not in a form I could use. Try asking again.';
      this.finish(text, reply, {});
    } catch (err) {
      clearInterval(tick);
      this.setBusy(false);
      if (err?.name === 'AbortError') {
        this.addBubble('assistant', 'Stopped.');
        return;
      }
      console.warn('[Xordor] AI request failed', err);
      const msg = err instanceof AIError ? err.message : `Something went wrong: ${err?.message || err}`;
      const hint = err instanceof AIError ? err.hint : '';
      this.addBubble('assistant error', hint ? `${msg} ${hint}` : msg,
        h('div', { class: 'btn-row' },
          h('button', { type: 'button', class: 'btn btn-small', onclick: () => this.openSettings() }, 'AI settings'),
          h('button', { type: 'button', class: 'btn btn-small', onclick: () => { this.input.value = text; this.send(); } }, 'Try again')));
    } finally {
      this.abort = null;
    }
  }

  finish(userText, reply, { offline }) {
    const ed = this.editor;
    const { project, summary, warnings, select } = applyActions(ed.project, reply.actions, { selectedId: ed.selectedId });
    let changed = false;
    if (hasChanges(summary)) changed = ed.commit(project, { source: 'ai', label: 'AI change' });
    const depth = ed.historyDepth;
    if (select && changed) ed.select(select);
    const what = describeSummary(summary);
    const extra = h('div', { class: 'bubble-extra' });
    if (changed && what) extra.append(h('p', { class: 'muted small' }, what));
    if (warnings.length) extra.append(h('p', { class: 'muted small' }, warnings.slice(0, 3).join(' ')));
    if (changed) {
      extra.append(h('div', { class: 'btn-row' },
        h('button', {
          type: 'button', class: 'btn btn-small', onclick: (ev) => {
            if (ed.historyDepth !== depth) { toast('Other changes happened after this one. Use Undo in the top bar.'); return; }
            ed.undo();
            ev.currentTarget.disabled = true;
            ev.currentTarget.textContent = 'Undone';
          },
        }, icon('undo'), 'Undo this'),
        h('button', { type: 'button', class: 'btn btn-small', onclick: () => ed.focusSelected() }, icon('focus'), 'Show me')));
    }
    if (offline && !changed && !reply.message.includes('AI settings')) {
      extra.append(h('p', { class: 'muted small' }, 'Tip: connect ChatGPT, Claude or Gemini in AI settings for smarter help.'));
    }
    this.addBubble('assistant', reply.message || (changed ? 'Done!' : 'Nothing to change.'), extra.childNodes.length ? extra : null);
    this.history.push({ role: 'user', content: userText });
    this.history.push({ role: 'assistant', content: JSON.stringify({ message: reply.message || '', applied: what || 'no changes' }) });
    if (this.history.length > 24) this.history.splice(0, this.history.length - 24);
  }

  openSettings() {
    const s = JSON.parse(JSON.stringify(this.settings));
    const body = h('div', { class: 'ai-settings' });
    const render = () => {
      clear(body);
      const id = s.provider;
      const def = PROVIDERS[id];
      const providerGrid = h('div', { class: 'provider-grid', role: 'radiogroup', 'aria-label': 'AI service' },
        Object.entries(PROVIDERS).map(([k, p]) => h('button', {
          type: 'button', role: 'radio', 'aria-checked': k === id ? 'true' : 'false',
          class: `provider-option ${k === id ? 'on' : ''}`,
          onclick: () => { s.provider = k; render(); },
        }, h('strong', {}, p.label))));
      body.append(h('p', { class: 'field-label' }, 'Which AI should help you?'), providerGrid);
      if (def.blurb) body.append(h('p', { class: 'muted' }, def.blurb));
      if (def.kind === 'offline') {
        body.append(h('p', { class: 'muted small' }, 'No account needed. For full creative help, pick ChatGPT, Claude, Gemini or another service and paste an API key.'));
        return;
      }
      if (def.needsKey || def.editableUrl) {
        const keyInput = h('input', { type: 'password', class: 'input', id: 'ai-key', value: s.keys[id] || '', placeholder: def.keyHint || 'Paste your API key', autocomplete: 'off', spellcheck: false });
        keyInput.addEventListener('input', () => { s.keys[id] = keyInput.value.trim(); });
        body.append(h('div', { class: 'field' },
          h('label', { class: 'field-label', for: 'ai-key' }, def.needsKey ? 'API key' : 'API key (optional)'),
          keyInput,
          h('p', { class: 'field-hint' }, 'Saved only in this browser and sent only to ', def.label, '. ',
            def.keyUrl ? h('a', { href: def.keyUrl, target: '_blank', rel: 'noopener noreferrer' }, 'Get a key') : null)));
      }
      if (def.editableUrl || id === 'custom') {
        const url = h('input', { type: 'url', class: 'input', id: 'ai-url', value: s.baseUrls[id] || def.baseUrl || '', placeholder: 'https://example.com/v1' });
        url.addEventListener('input', () => { s.baseUrls[id] = url.value.trim(); });
        body.append(h('div', { class: 'field' }, h('label', { class: 'field-label', for: 'ai-url' }, 'Server address'), url));
      }
      const listId = `models-${id}`;
      const model = h('input', { type: 'text', class: 'input', id: 'ai-model', value: s.models[id] ?? def.defaultModel ?? '', placeholder: 'Model name', list: listId, spellcheck: false });
      model.setAttribute('list', listId);
      model.addEventListener('input', () => { s.models[id] = model.value.trim(); });
      const datalist = h('datalist', { id: listId }, (def.models || []).map((m) => h('option', { value: m })));
      const status = h('p', { class: 'field-hint', 'aria-live': 'polite' });
      const loadBtn = h('button', {
        type: 'button', class: 'btn btn-small', onclick: async () => {
          status.textContent = 'Loading models...';
          try {
            const ids = await listModels(s);
            clear(datalist);
            for (const m of ids) datalist.append(h('option', { value: m }));
            status.textContent = ids.length ? `Found ${ids.length} models. Click the model box to pick one.` : 'No models found.';
            if (!model.value && ids[0]) { model.value = ids[0]; s.models[id] = ids[0]; }
          } catch (err) {
            status.textContent = err instanceof AIError ? `${err.message} ${err.hint || ''}` : `Couldn't load models: ${err.message}`;
          }
        },
      }, 'Load models');
      body.append(h('div', { class: 'field' }, h('label', { class: 'field-label', for: 'ai-model' }, 'Model'), h('div', { class: 'inline' }, model, loadBtn), datalist, status));
      if (def.kind === 'anthropic') {
        const effort = h('select', { class: 'input', id: 'ai-effort' }, [['low', 'Fast'], ['medium', 'Balanced'], ['high', 'Thorough']].map(([v, l]) => h('option', { value: v, selected: s.effort === v }, l)));
        effort.addEventListener('change', () => { s.effort = effort.value; });
        body.append(h('div', { class: 'field' }, h('label', { class: 'field-label', for: 'ai-effort' }, 'Thinking'), effort, h('p', { class: 'field-hint' }, 'Thorough builds more carefully but takes longer.')));
      }
      body.append(h('p', { class: 'muted small' }, 'Each request sends a description of your world (names, shapes, positions and rules) to the service you picked. It is charged to your account with that service.'));
    };
    render();
    openDialog({
      title: 'AI settings',
      body,
      wide: true,
      actions: [
        { label: 'Cancel', value: null },
        {
          label: 'Save', primary: true, run: () => {
            this.settings = s;
            saveAISettings(s);
            this.updateChip();
            toast(`Using ${PROVIDERS[s.provider].label}`);
            return true;
          },
        },
      ],
    });
  }
}
