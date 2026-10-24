// Thought log with the same rule as WASMKV: a newer txn on a key replaces
// the older one, and a snapshot read ignores txns after `asOf`.
// onCommit sees every new record (persist.js writes them to IndexedDB);
// replay puts records back on load without calling it.

const LIMIT = 800;

export function createStore(opts) {
  const onCommit = (opts && opts.onCommit) || null;
  let head = 0;
  const recs = [];
  return {
    head() { return head; },
    commit(page, key, value, limit) {
      head += 1;
      const rec = { page: String(page), key: String(key), txn: head, value: String(value).slice(0, limit || LIMIT) };
      recs.push(rec);
      if (onCommit) onCommit(rec);
      return head;
    },
    replay(txn, page, key, value) {
      const id = Number(txn);
      if (id > head) head = id;
      recs.push({ page: String(page), key: String(key), txn: id, value: String(value) });
    },
    read(page, key, asOf) {
      const seen = asOf == null ? head : asOf;
      let best = null;
      for (const r of recs) {
        if (r.page !== page || r.key !== key || r.txn > seen) continue;
        if (!best || r.txn > best.txn) best = r;
      }
      return best ? best.value : null;
    },
    // Every version of one key, oldest first.
    history(page, key) {
      return recs.filter((r) => r.page === page && r.key === key).sort((a, b) => a.txn - b.txn);
    },
    scan(page, prefix, asOf) {
      const seen = asOf == null ? head : asOf;
      const map = new Map();
      for (const r of recs) {
        if (r.page !== page || r.txn > seen || !r.key.startsWith(prefix)) continue;
        const cur = map.get(r.key);
        if (!cur || r.txn > cur.txn) map.set(r.key, r);
      }
      return [...map.values()].sort((a, b) => a.txn - b.txn);
    },
  };
}
