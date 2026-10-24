// Champ's local mind: MVCC thought versions, the DEC embedding module,
// and a library slot for each new weight or self-improvement note.

import { createStore } from "./mvcc.js";
import { buildLut, embedText, cosine, packEmbedModule } from "./embed.js";

const CHAT = { id: "champ", role: "chat", codec: "bnz" };
const EMBED = { id: "embed-dec", role: "embed", codec: "bnz" };

// opts.onCommit receives every new record, for persistence (persist.js).
export function createMind(opts) {
  const store = createStore(opts);
  const lut = buildLut();
  const embedBnz = packEmbedModule(lut);
  const mods = [CHAT, EMBED];
  return {
    store,
    lut,
    embedBnz,
    embed(text) { return embedText(text, lut); },
    cosine,
    commit(page, key, value, limit) { return store.commit(page, key, value, limit); },
    read(page, key, asOf) { return store.read(page, key, asOf); },
    history(page, key) { return store.history(page, key); },
    replay(txn, page, key, value) { store.replay(txn, page, key, value); },
    scan(page, prefix, asOf) { return store.scan(page, prefix, asOf); },
    head() { return store.head(); },
    modules() { return mods.map((m) => Object.assign({}, m)); },
    // After a replay: the library slots come back from their own records.
    restoreModules() {
      for (const rec of store.scan("library", "", store.head())) {
        if (mods.some((m) => m.id === rec.key)) continue;
        try { mods.push(JSON.parse(rec.value)); } catch { /* not a module record */ }
      }
    },
    addModule(mod) {
      mods.push(mod);
      store.commit("library", mod.id, JSON.stringify(mod));
      return mod;
    },
    learn(question, answer) {
      const text = (String(question) + "\n" + String(answer)).slice(0, 700);
      const txn = store.commit("learn", "note/" + (store.head() + 1), text);
      this.addModule({ id: "learn-" + txn, role: "memory", codec: "mvcc", page: "learn" });
      return txn;
    },
    // A page read to learn from: its text in pieces small enough to recall, each with its address.
    learnPage(source, text) {
      const body = String(text || "").replace(/\s+/g, " ").trim();
      let txn = 0;
      for (let at = 0; at < body.length && at < 2400; at += 600) {
        txn = store.commit("learn", "note/" + (store.head() + 1), (String(source) + "\n" + body.slice(at, at + 600)).slice(0, 700));
      }
      if (txn) this.addModule({ id: "learn-" + txn, role: "memory", codec: "mvcc", page: "learn" });
      return txn;
    },
    recall(query, minScore) {
      const floor = minScore == null ? 0.72 : minScore;
      const qv = embedText(query, lut);
      let best = "";
      let score = floor;
      for (const rec of store.scan("learn", "note/", store.head())) {
        const s = cosine(qv, embedText(rec.value, lut));
        if (s > score) {
          score = s;
          best = rec.value;
        }
      }
      return best ? best.slice(0, 400) : "";
    },
  };
}

export function thoughtPrompt(step, question, earlier) {
  const prior = earlier ? " So far: " + earlier.slice(0, 180) : "";
  return "One reasoning step only, in one sentence. Do not give the final answer. Do not use a tool." + prior + "\nQuestion: " + question + "\nStep " + step + ":";
}

export function explainPrompt(question, versions, notes) {
  const lines = versions.map((v, i) => (i + 1) + ". " + v).join("\n");
  const note = notes ? "\nNotes:\n" + notes : "";
  return "Explain the answer in plain words. Do not use a tool.\nThoughts:\n" + lines + note + "\nQuestion: " + question;
}
