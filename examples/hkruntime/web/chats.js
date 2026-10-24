// Chats in Champ's MVCC store, and the context that carries a chat across
// reloads and past a full context window.
//
// page "chat":  <id>/meta        {title, created, updated}   ("" = deleted)
//               <id>/m/<seq>     {role, text, card?, at}     ("" = removed)
// page "sum":   <id>/seg/<n>     {from, to, text}  one summary per stretch of turns
//               <id>/roll        the whole chat in a few sentences; every
//                                update is a new version (history() has them all)
//
// gather() builds what the model sees when its context is rebuilt: the rolling
// summary, the segment summaries closest to the new message, and the recent
// turns that no summary covers yet, inside a character budget.

const MSG_LIMIT = 32000;
const pad = (n, w) => String(n).padStart(w, "0");

export function newChatId() {
  const r = Math.floor(Math.random() * 0x1000000).toString(36);
  return Date.now().toString(36) + r;
}

export function titleOf(text) {
  const t = String(text || "").replace(/\s+/g, " ").trim().replace(/^\/\w+\s*/, "");
  if (!t) return "New chat";
  return t.length > 48 ? t.slice(0, 47).trimEnd() + "…" : t;
}

function parse(v) {
  if (!v) return null;
  try { return JSON.parse(v); } catch { return null; }
}

export function createChats(mind, opts) {
  const now = (opts && opts.now) || (() => Date.now());

  function meta(id) {
    return parse(mind.read("chat", id + "/meta"));
  }
  function putMeta(id, m) {
    mind.commit("chat", id + "/meta", JSON.stringify(m), MSG_LIMIT);
  }
  function messages(id) {
    return mind.scan("chat", id + "/m/")
      .map((r) => { const m = parse(r.value); return m && Object.assign({ seq: Number(r.key.slice(id.length + 3)) }, m); })
      .filter(Boolean)
      .sort((a, b) => a.seq - b.seq);
  }
  function nextSeq(id) {
    const all = mind.scan("chat", id + "/m/");
    let n = 0;
    for (const r of all) n = Math.max(n, Number(r.key.slice(id.length + 3)) + 1);
    return n;
  }
  function segments(id) {
    return mind.scan("sum", id + "/seg/")
      .map((r) => { const s = parse(r.value); return s && Object.assign({ n: Number(r.key.slice(id.length + 5)) }, s); })
      .filter(Boolean)
      .sort((a, b) => a.n - b.n);
  }

  return {
    list() {
      const out = [];
      for (const r of mind.scan("chat", "")) {
        if (!r.key.endsWith("/meta")) continue;
        const m = parse(r.value);
        if (m) out.push(Object.assign({ id: r.key.slice(0, -5) }, m));
      }
      return out.sort((a, b) => b.updated - a.updated);
    },
    meta,
    create(firstText) {
      const id = newChatId();
      const t = now();
      putMeta(id, { title: titleOf(firstText), created: t, updated: t });
      return id;
    },
    rename(id, title) {
      const m = meta(id);
      if (!m) return;
      putMeta(id, Object.assign(m, { title: titleOf(title), updated: m.updated }));
    },
    remove(id) {
      mind.commit("chat", id + "/meta", "");
    },
    messages,
    // msg: {role: "user"|"assistant"|"card"|"link", text, card, via, link}
    // via: the model that answered; link: a hand-off from one model to another (the continuity chain).
    add(id, msg) {
      const seq = nextSeq(id);
      const rec = { role: msg.role, text: String(msg.text || ""), at: now() };
      if (msg.card) rec.card = msg.card;
      if (msg.via) rec.via = msg.via;
      if (msg.link) rec.link = msg.link;
      mind.commit("chat", id + "/m/" + pad(seq, 6), JSON.stringify(rec), MSG_LIMIT);
      const m = meta(id);
      if (m) putMeta(id, Object.assign(m, { updated: rec.at }));
      return seq;
    },
    // Removes messages seq >= from (edit and regenerate). Older versions stay in the log.
    truncate(id, from) {
      for (const m of messages(id)) if (m.seq >= from) mind.commit("chat", id + "/m/" + pad(m.seq, 6), "");
      for (const s of segments(id)) if (s.to >= from) mind.commit("sum", id + "/seg/" + pad(s.n, 4), "");
    },
    segments,
    roll(id) {
      return mind.read("sum", id + "/roll") || "";
    },
    rollHistory(id) {
      return mind.history("sum", id + "/roll").map((r) => r.value).filter(Boolean);
    },
    setRoll(id, text) {
      mind.commit("sum", id + "/roll", String(text || "").trim(), 4000);
    },
    addSegment(id, from, to, text) {
      const segs = segments(id);
      const n = segs.length ? segs[segs.length - 1].n + 1 : 0;
      mind.commit("sum", id + "/seg/" + pad(n, 4), JSON.stringify({ from, to, text: String(text || "").trim() }), 4000);
      return n;
    },
    // The last message seq any segment summary covers, or -1.
    covered(id) {
      let to = -1;
      for (const s of segments(id)) to = Math.max(to, s.to);
      return to;
    },
    // Context for a rebuilt model: summaries plus the recent, unsummarized turns.
    // before: messages with seq >= before are left out (the message being sent).
    gather(id, query, before, budget) {
      const max = budget || 1800;
      const parts = [];
      let used = 0;
      const roll = this.roll(id);
      if (roll) {
        parts.push("Summary of this chat so far: " + roll);
        used += roll.length;
      }
      const segs = segments(id).filter((s) => s.text && s.text !== roll);
      if (segs.length) {
        const qv = mind.embed(query || "");
        const ranked = segs.map((s) => ({ s, score: mind.cosine(qv, mind.embed(s.text)) }))
          .sort((a, b) => b.score - a.score).slice(0, 2);
        const lines = [];
        for (const { s } of ranked) {
          if (used + s.text.length > max * 0.6) break;
          lines.push("- " + s.text);
          used += s.text.length;
        }
        if (lines.length) parts.push("Earlier parts of this chat:\n" + lines.join("\n"));
      }
      const from = this.covered(id) + 1;
      const recent = messages(id).filter((m) => m.seq >= from && (before == null || m.seq < before) && m.role !== "card" && m.text);
      const kept = [];
      for (let i = recent.length - 1; i >= 0; i--) {
        const line = (recent[i].role === "user" ? "You: " : "Champ: ") + recent[i].text.replace(/\s+/g, " ").trim();
        const clipped = line.length > 600 ? line.slice(0, 599) + "…" : line;
        if (used + clipped.length > max) break;
        kept.unshift(clipped);
        used += clipped.length;
      }
      if (kept.length) parts.push("Recent messages:\n" + kept.join("\n"));
      return parts.join("\n\n");
    },
  };
}

// The text of turns, for a summary written from the store (no model context left).
export function transcript(msgs, maxChars) {
  const max = maxChars || 2400;
  const lines = [];
  let used = 0;
  for (let i = msgs.length - 1; i >= 0; i--) {
    const m = msgs[i];
    if (m.role === "card" || !m.text) continue;
    const line = (m.role === "user" ? "You: " : "Champ: ") + m.text.replace(/\s+/g, " ").trim();
    if (used + line.length > max) break;
    lines.unshift(line);
    used += line.length;
  }
  return lines.join("\n");
}

// Asked inside the model's own context, which already holds the turns.
export function inContextSummaryPrompt() {
  return "Summarize our conversation so far in at most four sentences for your own notes. Keep names, numbers, decisions and open questions. Do not use a tool.";
}

export function segmentPrompt(turns) {
  return "Summarize this part of a chat in at most four sentences. Keep names, numbers, decisions and open questions. Do not use a tool.\n\n" + turns;
}

export function rollPrompt(prev, seg) {
  return "Merge these two summaries of one chat into a single summary of at most five sentences. Keep names, numbers, decisions and open questions. Do not use a tool.\n\nEarlier: " + prev + "\n\nLater: " + seg;
}
