// The MVCC log on this device: every record mind commits is appended to
// IndexedDB ("champ" / "mvcc", keyed by txn) and replayed on load, so chats,
// their summaries and learned notes survive a reload. Without IndexedDB
// (private window, blocked storage) the page still works for this visit.

const DB = "champ";
const STORE = "mvcc";

function openDB() {
  return new Promise((resolve) => {
    let req;
    try { req = indexedDB.open(DB, 1); } catch { resolve(null); return; }
    req.onupgradeneeded = () => req.result.createObjectStore(STORE, { keyPath: "txn" });
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => resolve(null);
    req.onblocked = () => resolve(null);
  });
}

export function createLog() {
  let db = null;
  let queue = [];
  const ready = openDB().then((d) => {
    db = d;
    const pending = queue;
    queue = null;
    if (db) for (const rec of pending) write(rec);
  });
  function write(rec) {
    if (!db) {
      if (queue) queue.push(rec);
      return;
    }
    try { db.transaction(STORE, "readwrite").objectStore(STORE).put(rec); } catch { /* storage full or closed */ }
  }
  return {
    ready,
    onCommit(rec) { write(rec); },
    // Replays every saved record into mind, in txn order.
    async load(mind) {
      await ready;
      if (!db) return 0;
      const recs = await new Promise((resolve) => {
        try {
          const req = db.transaction(STORE, "readonly").objectStore(STORE).getAll();
          req.onsuccess = () => resolve(req.result || []);
          req.onerror = () => resolve([]);
        } catch { resolve([]); }
      });
      recs.sort((a, b) => a.txn - b.txn);
      for (const r of recs) mind.replay(r.txn, r.page, r.key, r.value);
      return recs.length;
    },
  };
}
