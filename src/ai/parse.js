// Reads the AI's reply. Models sometimes wrap JSON in ``` fences, add text
// around it, or leave trailing commas, so this is deliberately forgiving.

function findBalanced(text, start) {
  let depth = 0;
  let inString = false;
  let escape = false;
  for (let i = start; i < text.length; i++) {
    const c = text[i];
    if (inString) {
      if (escape) escape = false;
      else if (c === '\\') escape = true;
      else if (c === '"') inString = false;
      continue;
    }
    if (c === '"') inString = true;
    else if (c === '{' || c === '[') depth++;
    else if (c === '}' || c === ']') {
      depth--;
      if (depth === 0) return i;
    }
  }
  return -1;
}

function tryParse(s) {
  try {
    return JSON.parse(s);
  } catch {
    try {
      // Remove trailing commas and comments, a common slip.
      const cleaned = s.replace(/\/\/[^\n"]*$/gm, '').replace(/,\s*([}\]])/g, '$1');
      return JSON.parse(cleaned);
    } catch {
      return undefined;
    }
  }
}

/** Returns { message, actions } from raw model text. Never throws. */
export function parseAssistantReply(text) {
  const raw = String(text ?? '').trim();
  if (!raw) return { message: '', actions: [], ok: false };
  const candidates = [];
  const fence = raw.match(/```(?:json)?\s*([\s\S]*?)```/i);
  if (fence) candidates.push(fence[1].trim());
  candidates.push(raw);
  for (const c of candidates) {
    for (let start = c.indexOf('{'); start !== -1; start = c.indexOf('{', start + 1)) {
      const end = findBalanced(c, start);
      if (end === -1) break;
      const value = tryParse(c.slice(start, end + 1));
      if (value && typeof value === 'object' && !Array.isArray(value) && ('actions' in value || 'message' in value)) {
        const actions = Array.isArray(value.actions) ? value.actions : value.action ? [value.action] : [];
        const message = typeof value.message === 'string' ? value.message : typeof value.reply === 'string' ? value.reply : '';
        return { message, actions, ok: true };
      }
    }
    // A bare list of actions.
    const arrStart = c.indexOf('[');
    if (arrStart !== -1) {
      const end = findBalanced(c, arrStart);
      if (end !== -1) {
        const value = tryParse(c.slice(arrStart, end + 1));
        if (Array.isArray(value) && value.every((v) => v && typeof v === 'object')) return { message: '', actions: value, ok: true };
      }
    }
  }
  return { message: raw.slice(0, 2000), actions: [], ok: false };
}
