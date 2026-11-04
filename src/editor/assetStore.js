// Keeps imported models in the app's own database (IndexedDB), so a project
// can be recovered after a crash even when its models are too big for the
// small recovery copy. Every function fails quietly: this is only a backup.
const DB_NAME = 'xordor';
const STORE = 'assets';

function openDb() {
  return new Promise((resolve, reject) => {
    if (typeof indexedDB === 'undefined') { reject(new Error('No IndexedDB')); return; }
    const req = indexedDB.open(DB_NAME, 1);
    req.onupgradeneeded = () => req.result.createObjectStore(STORE, { keyPath: 'id' });
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => reject(req.error);
  });
}

function done(tx) {
  return new Promise((resolve, reject) => {
    tx.oncomplete = () => resolve();
    tx.onerror = () => reject(tx.error);
    tx.onabort = () => reject(tx.error);
  });
}

export async function storeAssets(list) {
  if (!list?.length) return;
  try {
    const db = await openDb();
    const tx = db.transaction(STORE, 'readwrite');
    for (const a of list) tx.objectStore(STORE).put(a);
    await done(tx);
    db.close();
  } catch { /* recovery copy only */ }
}

export async function loadAssets(ids) {
  if (!ids?.length) return [];
  try {
    const db = await openDb();
    const tx = db.transaction(STORE, 'readonly');
    const store = tx.objectStore(STORE);
    const found = await Promise.all(ids.map((id) => new Promise((resolve) => {
      const r = store.get(id);
      r.onsuccess = () => resolve(r.result || null);
      r.onerror = () => resolve(null);
    })));
    db.close();
    return found.filter(Boolean);
  } catch {
    return [];
  }
}
