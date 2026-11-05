// Connects to AI services. In the desktop app this runs in the main process,
// where keys are stored; in a browser it runs in the page. Either way the key
// is sent only to the provider you pick.
import Anthropic from '@anthropic-ai/sdk';

export const PROVIDERS = {
  offline: {
    label: 'Built-in helper',
    blurb: 'Free and works offline. Understands simple requests like "add 5 red cubes" or "make it night".',
    kind: 'offline',
    needsKey: false,
  },
  openai: {
    label: 'ChatGPT (OpenAI)',
    blurb: 'Uses your OpenAI API key.',
    kind: 'openai',
    baseUrl: 'https://api.openai.com/v1',
    defaultModel: 'gpt-5.5',
    models: ['gpt-5.5', 'gpt-5.4-mini', 'gpt-5.4-nano'],
    keyUrl: 'https://platform.openai.com/api-keys',
    keyHint: 'sk-...',
    needsKey: true,
  },
  anthropic: {
    label: 'Claude (Anthropic)',
    blurb: 'Uses your Anthropic API key.',
    kind: 'anthropic',
    defaultModel: 'claude-opus-5-5',
    models: ['claude-opus-5-5', 'claude-sonnet-5-5', 'claude-haiku-5-5', 'claude-fable-5-1'],
    keyUrl: 'https://platform.claude.com/settings/keys',
    keyHint: 'sk-ant-...',
    needsKey: true,
  },
  gemini: {
    label: 'Gemini (Google)',
    blurb: 'Uses your Google AI Studio API key.',
    kind: 'gemini',
    baseUrl: 'https://generativelanguage.googleapis.com/v1beta',
    defaultModel: 'gemini-3.8-flash',
    models: ['gemini-3.8-flash', 'gemini-3.5-flash-lite', 'gemini-4-argon'],
    keyUrl: 'https://aistudio.google.com/apikey',
    keyHint: 'AIza...',
    needsKey: true,
  },
  openrouter: {
    label: 'OpenRouter (many models)',
    blurb: 'One key for hundreds of models from many companies.',
    kind: 'openai',
    baseUrl: 'https://openrouter.ai/api/v1',
    defaultModel: 'openrouter/auto',
    models: ['openrouter/auto'],
    keyUrl: 'https://openrouter.ai/keys',
    keyHint: 'sk-or-...',
    needsKey: true,
  },
  deepseek: {
    label: 'DeepSeek',
    kind: 'openai',
    baseUrl: 'https://api.deepseek.com/v1',
    defaultModel: 'deepseek-chat',
    models: ['deepseek-chat'],
    keyUrl: 'https://platform.deepseek.com/api_keys',
    needsKey: true,
  },
  mistral: {
    label: 'Mistral',
    kind: 'openai',
    baseUrl: 'https://api.mistral.ai/v1',
    defaultModel: 'mistral-large-latest',
    models: ['mistral-large-latest', 'mistral-small-latest'],
    keyUrl: 'https://console.mistral.ai/api-keys',
    needsKey: true,
  },
  groq: {
    label: 'Groq',
    kind: 'openai',
    baseUrl: 'https://api.groq.com/openai/v1',
    defaultModel: '',
    models: [],
    keyUrl: 'https://console.groq.com/keys',
    needsKey: true,
  },
  xai: {
    label: 'Grok (xAI)',
    kind: 'openai',
    baseUrl: 'https://api.x.ai/v1',
    defaultModel: '',
    models: [],
    keyUrl: 'https://console.x.ai',
    needsKey: true,
  },
  ollama: {
    label: 'Ollama (free, on your computer)',
    blurb: 'Runs AI models on your own computer. Install Ollama, then start it with OLLAMA_ORIGINS=* so this page can reach it.',
    kind: 'openai',
    baseUrl: 'http://localhost:11434/v1',
    defaultModel: 'llama3.2',
    models: [],
    keyUrl: 'https://ollama.com/download',
    needsKey: false,
    editableUrl: true,
  },
  lmstudio: {
    label: 'LM Studio (free, on your computer)',
    blurb: 'Start the local server in LM Studio and turn on CORS.',
    kind: 'openai',
    baseUrl: 'http://localhost:1234/v1',
    defaultModel: '',
    models: [],
    keyUrl: 'https://lmstudio.ai',
    needsKey: false,
    editableUrl: true,
  },
  custom: {
    label: 'Other (OpenAI-compatible)',
    blurb: 'Any service that offers an OpenAI-compatible /chat/completions endpoint.',
    kind: 'openai',
    baseUrl: '',
    defaultModel: '',
    models: [],
    needsKey: false,
    editableUrl: true,
  },
};

// The same code runs in the desktop app's main process (Node) and in a browser.
const IN_BROWSER = typeof window !== 'undefined' && typeof document !== 'undefined';

// Claude models that accept server-side refusal fallbacks ("default" routing).
const CLAUDE_FALLBACK_MODELS = new Set(['claude-opus-5-5', 'claude-opus-5', 'claude-fable-5-1', 'claude-sonnet-5-5']);
// Claude models that accept output_config.effort.
const CLAUDE_EFFORT = /^claude-(opus-5|sonnet-5|haiku-5|fable-5|mythos-5|opus-4-[678]|sonnet-4-6)/;

export class AIError extends Error {
  constructor(message, { status = 0, hint = '' } = {}) {
    super(message);
    this.status = status;
    this.hint = hint;
  }
}

function friendlyHttpError(providerLabel, status, detail) {
  if (status === 401 || status === 403) return new AIError(`${providerLabel} did not accept your API key.`, { status, hint: 'Open AI settings and check the key.' });
  if (status === 404) return new AIError(`${providerLabel} could not find that model.`, { status, hint: 'Open AI settings and pick another model, or press "Load models".' });
  if (status === 429) return new AIError(`${providerLabel} says you are sending too much right now, or your account is out of credit.`, { status, hint: 'Wait a minute, or check your billing page.' });
  if (status >= 500) return new AIError(`${providerLabel} is having trouble right now.`, { status, hint: 'Try again in a moment.' });
  return new AIError(`${providerLabel} returned an error${detail ? `: ${detail}` : '.'}`, { status });
}

function networkError(providerLabel, cfg) {
  const local = /localhost|127\.0\.0\.1/.test(cfg.baseUrl || '');
  return new AIError(`Couldn't reach ${providerLabel}.`, {
    hint: local
      ? 'Make sure the app is running on your computer and allows requests from this page (for Ollama: OLLAMA_ORIGINS=*).'
      : 'Check your internet connection. Some services block requests made directly from a web page.',
  });
}

async function readError(res) {
  try {
    const data = await res.json();
    return data?.error?.message || data?.message || (typeof data?.error === 'string' ? data.error : '');
  } catch {
    return '';
  }
}

function resolve(settings) {
  const id = settings.provider || 'offline';
  const def = PROVIDERS[id] || PROVIDERS.offline;
  return {
    id,
    def,
    apiKey: (settings.keys?.[id] || '').trim(),
    model: (settings.models?.[id] || def.defaultModel || '').trim(),
    baseUrl: (settings.baseUrls?.[id] || def.baseUrl || '').replace(/\/+$/, ''),
    effort: settings.effort || 'medium',
  };
}

export function describeProvider(settings) {
  const cfg = resolve(settings);
  return { label: cfg.def.label, model: cfg.def.kind === 'offline' ? '' : cfg.model, kind: cfg.def.kind };
}

export function isReady(settings) {
  const cfg = resolve(settings);
  if (cfg.def.kind === 'offline') return { ok: true };
  if (cfg.def.needsKey && !cfg.apiKey) return { ok: false, reason: `Add your ${cfg.def.label} API key in AI settings.` };
  if (!cfg.baseUrl && cfg.def.kind === 'openai') return { ok: false, reason: 'Add the server address in AI settings.' };
  if (!cfg.model) return { ok: false, reason: 'Pick a model in AI settings (press "Load models").' };
  return { ok: true };
}

/**
 * Sends a chat to the chosen provider and returns the raw reply text.
 * opts: { system, messages: [{role, content}], signal, onProgress(chars) }
 */
export async function chat(settings, { system, messages, signal, onProgress }) {
  const cfg = resolve(settings);
  const ready = isReady(settings);
  if (!ready.ok) throw new AIError(ready.reason);
  switch (cfg.def.kind) {
    case 'anthropic': return chatAnthropic(cfg, { system, messages, signal, onProgress });
    case 'gemini': return chatGemini(cfg, { system, messages, signal });
    case 'openai': return chatOpenAI(cfg, { system, messages, signal });
    default: throw new AIError('This helper does not use an online service.');
  }
}

async function chatOpenAI(cfg, { system, messages, signal }) {
  const headers = { 'Content-Type': 'application/json' };
  if (cfg.apiKey) headers.Authorization = `Bearer ${cfg.apiKey}`;
  if (cfg.id === 'openrouter') {
    headers['X-Title'] = 'Xordor';
    try { headers['HTTP-Referer'] = location.origin.startsWith('http') ? location.origin : 'https://xordor.app'; } catch { /* ignore */ }
  }
  let res;
  try {
    res = await fetch(`${cfg.baseUrl}/chat/completions`, {
      method: 'POST',
      headers,
      signal,
      body: JSON.stringify({ model: cfg.model, messages: [{ role: 'system', content: system }, ...messages] }),
    });
  } catch (err) {
    if (err.name === 'AbortError') throw err;
    throw networkError(cfg.def.label, cfg);
  }
  if (!res.ok) throw friendlyHttpError(cfg.def.label, res.status, await readError(res));
  const data = await res.json();
  const content = data?.choices?.[0]?.message?.content;
  if (Array.isArray(content)) return content.map((c) => c.text || '').join('');
  if (typeof content !== 'string') throw new AIError(`${cfg.def.label} sent back an empty reply.`);
  return content;
}

async function chatGemini(cfg, { system, messages, signal }) {
  const contents = messages.map((m) => ({ role: m.role === 'assistant' ? 'model' : 'user', parts: [{ text: m.content }] }));
  let res;
  try {
    res = await fetch(`${cfg.baseUrl}/models/${encodeURIComponent(cfg.model)}:generateContent`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', 'x-goog-api-key': cfg.apiKey },
      signal,
      body: JSON.stringify({
        systemInstruction: { parts: [{ text: system }] },
        contents,
        generationConfig: { responseMimeType: 'application/json' },
      }),
    });
  } catch (err) {
    if (err.name === 'AbortError') throw err;
    throw networkError(cfg.def.label, cfg);
  }
  if (!res.ok) throw friendlyHttpError(cfg.def.label, res.status, await readError(res));
  const data = await res.json();
  const cand = data?.candidates?.[0];
  if (!cand) {
    const reason = data?.promptFeedback?.blockReason;
    throw new AIError(reason ? `Gemini declined this request (${reason}).` : 'Gemini sent back an empty reply.');
  }
  return (cand.content?.parts || []).map((p) => p.text || '').join('');
}

async function chatAnthropic(cfg, { system, messages, signal, onProgress }) {
  const client = new Anthropic({ apiKey: cfg.apiKey, dangerouslyAllowBrowser: IN_BROWSER, maxRetries: 1 });
  const params = {
    model: cfg.model,
    max_tokens: 64000,
    // The system prompt never changes, so it can be cached between requests.
    system: [{ type: 'text', text: system, cache_control: { type: 'ephemeral' } }],
    messages,
  };
  if (CLAUDE_EFFORT.test(cfg.model)) params.output_config = { effort: cfg.effort };
  if (CLAUDE_FALLBACK_MODELS.has(cfg.model)) {
    // If a safety check declines the request, Anthropic re-runs it on a suitable model.
    params.betas = ['server-side-fallback-2026-07-01'];
    params.fallbacks = 'default';
  }
  let message;
  try {
    const stream = client.beta.messages.stream(params, { signal });
    let chars = 0;
    stream.on('text', (t) => {
      chars += t.length;
      onProgress?.(chars);
    });
    message = await stream.finalMessage();
  } catch (err) {
    if (err?.name === 'AbortError' || err instanceof Anthropic.APIUserAbortError) throw Object.assign(new Error('aborted'), { name: 'AbortError' });
    if (err instanceof Anthropic.AuthenticationError || err instanceof Anthropic.PermissionDeniedError) throw friendlyHttpError(cfg.def.label, err.status);
    if (err instanceof Anthropic.NotFoundError) throw friendlyHttpError(cfg.def.label, 404);
    if (err instanceof Anthropic.RateLimitError) throw friendlyHttpError(cfg.def.label, 429);
    if (err instanceof Anthropic.APIConnectionError) throw networkError(cfg.def.label, cfg);
    if (err instanceof Anthropic.APIError) throw friendlyHttpError(cfg.def.label, err.status || 0, err.error?.error?.message || err.message);
    throw err;
  }
  if (message.stop_reason === 'refusal') {
    throw new AIError('Claude declined this request.', { hint: message.stop_details?.explanation || 'Try asking in a different way.' });
  }
  // After a fallback, only the text after the last fallback marker belongs to the final answer.
  const blocks = message.content;
  let start = 0;
  blocks.forEach((b, i) => { if (b.type === 'fallback') start = i + 1; });
  const text = blocks.slice(start).filter((b) => b.type === 'text').map((b) => b.text).join('');
  if (!text && message.stop_reason === 'max_tokens') throw new AIError('The reply was too long and got cut off.', { hint: 'Try asking for a smaller change.' });
  return text;
}

/** Lists the models a provider offers, so people can pick one. */
export async function listModels(settings) {
  const cfg = resolve(settings);
  if (cfg.def.kind === 'offline') return [];
  if (cfg.def.needsKey && !cfg.apiKey) throw new AIError('Add your API key first.');
  try {
    if (cfg.def.kind === 'anthropic') {
      const client = new Anthropic({ apiKey: cfg.apiKey, dangerouslyAllowBrowser: IN_BROWSER, maxRetries: 0 });
      const ids = [];
      for await (const m of client.models.list({ limit: 100 })) ids.push(m.id);
      return ids;
    }
    if (cfg.def.kind === 'gemini') {
      const res = await fetch(`${cfg.baseUrl}/models?pageSize=200`, { headers: { 'x-goog-api-key': cfg.apiKey } });
      if (!res.ok) throw friendlyHttpError(cfg.def.label, res.status, await readError(res));
      const data = await res.json();
      return (data.models || [])
        .filter((m) => (m.supportedGenerationMethods || []).includes('generateContent'))
        .map((m) => m.name.replace(/^models\//, ''));
    }
    const headers = cfg.apiKey ? { Authorization: `Bearer ${cfg.apiKey}` } : {};
    const res = await fetch(`${cfg.baseUrl}/models`, { headers });
    if (!res.ok) throw friendlyHttpError(cfg.def.label, res.status, await readError(res));
    const data = await res.json();
    return (data.data || data.models || []).map((m) => m.id || m.name).filter(Boolean).sort();
  } catch (err) {
    if (err instanceof AIError) throw err;
    if (err instanceof Anthropic.AuthenticationError) throw friendlyHttpError(cfg.def.label, 401);
    if (err instanceof Anthropic.APIError) throw friendlyHttpError(cfg.def.label, err.status || 0, err.message);
    throw networkError(cfg.def.label, cfg);
  }
}
