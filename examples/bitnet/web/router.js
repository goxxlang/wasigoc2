// Champ's router between models and tools, laid out as jev_mesh_2x2
// (shuttle-repos/jev-mesh-2x2-sky130a): four engines on a 2x2 grid and the die's three ROM tables.
//
//            col 0 (small)       col 1 (large)
//   row 0    e0_0 Lite           e0_1 Balanced
//   row 1    e1_0 Thinking       e1_1 Pro+Think        row 1 thinks first
//
// A beat is one thing offered to the router, with the die's four kinds:
//   0 chat   (activation)  a message for an engine
//   1 tool   (weight)      a tool's result, loaded into the engine's context
//   2 reload               a model switch: the engine's context is cleared, the continuity chain stays
//   3 fence                the end of a turn
//
// The gate is the MoE gate of the monodromy note (Encryptid/monodromy/paper): the message gives two
// Bernoulli bits, think (row) and size (col), and their products are the four engines' full
// probabilities (they sum to 1). Top-1 as in Switch Transformers; engines the server does not have
// drop out and the kept gates are renormalized as in GShard. Switching models has a capacity: a
// message whose winner beats the loaded engine by less than CAPACITY follows the residual and stays on
// the loaded engine. The continuity chain (chats.gather) is the shared expert every engine sees.
// A tool's result is admitted into the context as hodgemix --update admits an expert: only when it
// moves the same way as the question (positive cosine); otherwise it stays on its card.

export const ENGINES = [
  { tier: "lite", label: "Lite", row: 0, col: 0 },
  { tier: "balanced", label: "Balanced", row: 0, col: 1 },
  { tier: "thinking", label: "Thinking", row: 1, col: 0 },
  { tier: "pro", label: "Pro+Think", row: 1, col: 1 },
];
export const TIERS = new Set(ENGINES.map((e) => e.tier));
export const KIND = { chat: 0, tool: 1, reload: 2, fence: 3 };

// jev_route: {full, kind} -> action. The die's ROM, states 0 to 7.
//   stage  run on the engine        load  load into the engine's context
//   reset  clear the engine         pass  send it on (to the card, the chain)
export const ROUTE = ["stage", "load", "reset", "pass", "stage", "pass", "reset", "pass"];
export function route(full, kind) {
  return ROUTE[(full ? 4 : 0) | (kind & 3)];
}

// jev_accept: take or stall. A tool beat is taken when offered; a chat beat when the queue has room
// and the engine can take it (loaded, idle); a reload or fence when the queue has room.
export const DEPTH = 2;
export function accept(kind, s) {
  const room = (s.queue || 0) < DEPTH;
  if (kind === KIND.tool) return "take";
  if (kind === KIND.chat) return room && !s.busy && !s.loading ? "take" : "stall";
  return room ? "take" : "stall";
}

// jev_join: hold or emit. Emit on the clock where the engine's result, the west partials (the
// continuity chain carried into this engine) and east room (a place on the page) are all present.
export const JOIN = ["hold", "hold", "hold", "hold", "hold", "hold", "hold", "emit"];
export function join(result, partials, room) {
  return JOIN[(result ? 1 : 0) | (partials ? 2 : 0) | (room ? 4 : 0)];
}

// ---- the two bits

const THINK = [
  [/\b(why|how come)\b/i, 0.3, "asks why"],
  [/\b(prove|proof|derive|reason(?:ing)?|step[- ]by[- ]step|work (?:it )?out|think (?:it )?through)\b/i, 0.35, "reasoning"],
  [/\b(compare|trade-?offs?|pros and cons|plan|strategy|debug|diagnose|analy[sz]e|evaluate|solve|calculate|estimate|what if)\b/i, 0.3, "analysis"],
  [/\d\s*[-+*/^×÷=]\s*\d|\b(integral|derivative|equation|theorem|probability|algorithm)\b/i, 0.3, "math"],
  [/```|\b(function|stack trace|exception|compile|regex|sql|segfault)\b/i, 0.25, "code"],
];
const SIZE = [
  [/\b(detailed|thorough|in depth|comprehensive|essay|report|article|story|chapter|full|complete)\b/i, 0.3, "long form"],
  [/\b(architecture|design|system|protocol|specification|research|paper|explain)\b/i, 0.2, "broad"],
  [/https?:\/\//i, 0.1, "a page"],
];
const PLAIN = /^(hi|hey|hello|thanks|thank you|ok|okay|yes|no|sure|cool|nice|good (?:morning|night|evening))\b[\s!.,?]*$/i;

const clamp = (x) => Math.max(0, Math.min(1, x));

// features: the think and size probabilities of a message, and the cues that set them.
export function features(text, opts) {
  const o = opts || {};
  const t = String(text || "").trim();
  const cues = [];
  let think = 0.1;
  let size = 0.1;
  if (PLAIN.test(t)) return { think: 0.02, size: 0.02, cues: ["plain"] };
  for (const [re, w, cue] of THINK) if (re.test(t)) { think += w; cues.push(cue); }
  for (const [re, w, cue] of SIZE) if (re.test(t)) { size += w; cues.push(cue); }
  const words = t ? t.split(/\s+/).length : 0;
  if (words > 60) { size += 0.25; cues.push(words + " words"); }
  else if (words > 25) size += 0.12;
  const asks = (t.match(/\?/g) || []).length;
  if (asks > 1) { think += 0.15; size += 0.1; cues.push(asks + " questions"); }
  const lines = t.split(/\n/).filter((l) => /^\s*(?:[-*]|\d+[.)])\s+/.test(l)).length;
  if (lines > 2) { size += 0.15; cues.push(lines + " parts"); }
  if (o.think) { think = Math.max(think, 0.9); cues.push("Think on"); }
  return { think: clamp(think), size: clamp(size), cues };
}

// gate: every engine's probability, p(row) * p(col).
export function gate(f) {
  return ENGINES.map((e) => (e.row ? f.think : 1 - f.think) * (e.col ? f.size : 1 - f.size));
}

// One model is loaded, so the other three engines sit idle. Their share of the
// gate is a presence penalty on tokens already said. The guest samples at the
// model's temperature and top-p, and stacks that penalty once per extra copy.
// Capped so a long message only nudges the first sighting.
export const PRESENCE_CAP = 0.3;
export function presenceOf(text, opts) {
  const g = gate(features(text, opts));
  const lite = ENGINES.findIndex((e) => e.tier === "lite");
  const idle = 1 - g[lite];
  return Math.round(Math.min(PRESENCE_CAP, idle * 0.5) * 1000) / 1000;
}

// mixOf is the four engine gates, in engine order. One model is loaded, so the
// guest draws each token's temperature from these weights. This is not a tool
// choice and it does not change which model decide() keeps.
export function mixOf(text, opts) {
  return gate(features(text, opts)).map((x) => Math.round(x * 1000) / 1000);
}

// ---- tools: strong cues only; the + picks always win

const TOOL_CUES = [
  ["notes", /\b(save|write|keep|put)\b.{0,40}\b(note|notes)\b|\bmy notes\b/i],
  ["tasks", /\b(add|create|make)\b.{0,30}\b(task|to-?do)\b|\bmy (tasks|to-?do list)\b|\bmark\b.{0,30}\bdone\b/i],
  ["canvas", /\b(draw|sketch|diagram)\b|\bcanvas\b/i],
  ["exec", /\brun\b.{0,20}\b(command|script|code|this)\b|^\s*\$\s+\S/i],
  ["gather", /\b(read|open|summari[sz]e|look at)\b.{0,40}https?:\/\//i],
];

export function toolsFor(text, picks) {
  const out = [...(picks || [])];
  for (const [tool, re] of TOOL_CUES) if (!out.includes(tool) && re.test(String(text || ""))) out.push(tool);
  return out;
}

// ---- the decision

export const CAPACITY = 0.2;

// models: [{id, name, tier}] from the catalog. mode: "auto" or a model id. loaded: the id in the guest.
// Returns {model, engine, think, gates, cues, tools, stay, swap, reason}.
export function decide(o) {
  const models = o.models || [];
  const tools = toolsFor(o.text, o.picks);
  const byTier = new Map();
  for (const m of models) if (TIERS.has(m.tier) && !byTier.has(m.tier)) byTier.set(m.tier, m);
  if (o.mode && o.mode !== "auto") {
    const m = models.find((x) => x.id === o.mode) || models.find((x) => x.id === o.loaded) || models[0] || null;
    const e = m && ENGINES.find((x) => x.tier === m.tier);
    return { model: m, engine: e || null, think: !!o.think, gates: null, cues: [], tools, stay: !!m && m.id === o.loaded, swap: !!m && m.id !== o.loaded, reason: m ? m.name : "" };
  }
  const f = features(o.text, { think: o.think });
  const g = gate(f);
  // Engines the server has keep their gates, renormalized.
  const kept = ENGINES.map((e, i) => (byTier.has(e.tier) ? g[i] : 0));
  const total = kept.reduce((a, b) => a + b, 0);
  if (!total) {
    const m = models.find((x) => x.id === o.loaded) || models[0] || null;
    return { model: m, engine: null, think: !!o.think, gates: g, cues: f.cues, tools, stay: true, swap: false, reason: m ? m.name : "" };
  }
  const p = kept.map((x) => x / total);
  let top = 0;
  for (let i = 1; i < p.length; i++) if (p[i] > p[top]) top = i;
  const want = ENGINES[g.indexOf(Math.max(...g))];
  let pick = top;
  // Capacity: a switch costs a model load. Close calls follow the residual (the loaded engine).
  const loaded = models.find((x) => x.id === o.loaded);
  const li = loaded ? ENGINES.findIndex((e) => e.tier === loaded.tier) : -1;
  const stay = li >= 0 && li !== top && p[li] > 0 && p[top] - p[li] < CAPACITY;
  if (stay) pick = li;
  const e = ENGINES[pick];
  const m = byTier.get(e.tier);
  const parts = [e.label + " " + p[pick].toFixed(2)];
  if (f.cues.length) parts.push(f.cues.slice(0, 3).join(", "));
  if (want.tier !== e.tier && !byTier.has(want.tier)) parts.push(want.label + " is not on this server");
  if (stay) parts.push("kept loaded (" + ENGINES[top].label + " " + p[top].toFixed(2) + ")");
  if (tools.length) parts.push("tools: " + tools.join(", "));
  return {
    model: m, engine: e, think: e.row === 1 || !!o.think, gates: p, cues: f.cues, tools,
    stay: m.id === o.loaded, swap: m.id !== o.loaded, reason: parts.join(" · "),
  };
}

// admit: hodgemix --update's rule for a tool result. The result joins the context when it moves the
// same way as the question; otherwise the engine answers alone and the result stays on its card.
export function admit(cos) {
  return Number.isFinite(cos) && cos > 0;
}
