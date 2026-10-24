// Champ extensions. A tool call is one JSON object. It may be the whole reply
// or sitting inside one. While it is still arriving, the page shows a loader
// (router.js: a tool beat routes to "load") and does not print the JSON.
// Anything that is not a tool call stays on screen as words.
//
// notes, tasks and canvas run in this page (GitMesh, TodoTraq and MotionKB,
// brought into the browser by localsvc.js): no sign-in, and what they keep
// stays on this device. exec runs in an EncryptidVM sandbox on this machine,
// after the person allows it.

import { createServices } from "./localsvc.js";
import { toolsFor } from "./router.js";

const AGENT = new Set(["notes", "tasks", "canvas", "exec"]);
const TOOLS = new Set(["datetime", "search", "gather", ...AGENT]);
const WRITES = new Set(["write", "save", "put", "rm", "remove", "delete", "add", "new", "create", "update", "set", "done", "start", "close", "comment", "draw"]);
const MAX_ROUNDS = 4;

export function isAgentTool(tool) {
  return AGENT.has(tool);
}

// Direct commands, so a tool works without the model writing JSON:
//   /run <command>            /notes [prefix]      /note <path>[⏎text]   /rm <path>
//   /tasks [todo|doing|done]  /task <title>        /done <id>   /start <id>
//   /canvas <title>[⏎markdown]   /canvases
export function slashCall(text) {
  const s = String(text || "");
  const m = /^\/([a-z]+)(?:[ \t]+([^\n]*))?(?:\n([\s\S]*))?$/i.exec(s.trim());
  if (!m) return null;
  const verb = m[1].toLowerCase();
  const arg = (m[2] || "").trim();
  const body = m[3];
  const call = (tool, args) => ({ type: "tool_call", tool, args });
  switch (verb) {
    case "run": case "exec": return arg || body ? call("exec", { cmd: (arg + (body ? "\n" + body : "")).trim() }) : null;
    case "notes": return call("notes", { op: "list", path: arg });
    case "note": return arg ? call("notes", body != null ? { op: "write", path: arg, text: body } : { op: "read", path: arg }) : null;
    case "rm": return arg ? call("notes", { op: "rm", path: arg }) : null;
    case "tasks": return call("tasks", { op: "list", state: arg });
    case "task": return arg ? call("tasks", { op: "add", title: arg, body: body || "" }) : null;
    case "done": return arg ? call("tasks", { op: "done", id: arg }) : null;
    case "start": return arg ? call("tasks", { op: "start", id: arg }) : null;
    case "canvas": return arg ? call("canvas", body != null ? { op: "write", title: arg, text: body } : { op: "read", title: arg }) : null;
    case "canvases": return call("canvas", { op: "list" });
    default: return null;
  }
}

export function clockText() {
  return new Date().toLocaleString(undefined, {
    weekday: "long", year: "numeric", month: "long", day: "numeric",
    hour: "numeric", minute: "2-digit", second: "2-digit", timeZoneName: "short",
  });
}

const TOOL_NAMES = ["datetime", "search", "gather", "shell", "notes", "tasks", "canvas", "exec"];

function argsOf(obj) {
  let raw = obj.args != null ? obj.args : (obj.arguments != null ? obj.arguments : obj.parameters);
  if (typeof raw === "string") {
    try { raw = JSON.parse(raw); } catch { raw = null; }
  }
  if (!raw || typeof raw !== "object" || Array.isArray(raw)) return null;
  return raw;
}

// One reply, already trimmed to a single object or the datetime shortcut.
function parseCall(text) {
  let s = (text || "").trim();
  const fence = /^```(?:json)?\s*([\s\S]*?)\s*```$/i.exec(s);
  if (fence) s = fence[1].trim();
  s = s.replace(/^\s*champ\s*[,:]\s*/i, "");
  if (s === "tool:datetime") return { type: "tool_call", tool: "datetime", args: {} };
  if (!s.startsWith("{") || !s.endsWith("}")) return null;
  let obj = null;
  try { obj = JSON.parse(s); } catch { obj = null; }
  if (obj && typeof obj === "object") {
    const tool = typeof obj.tool === "string" ? obj.tool : (typeof obj.name === "string" ? obj.name : "");
    const typed = obj.type === "tool_call";
    if (tool && (typed || TOOLS.has(tool))) {
      if (!TOOLS.has(tool)) return { type: "tool_call", tool, args: {} };
      const args = argsOf(obj);
      if (!typed && !args) return null;
      return { type: "tool_call", tool, args: args || {} };
    }
  }
  if (!/tool_call/.test(s)) return null;
  const named = /tool\s*[:=]\s*"?(datetime|search|gather|shell|notes|tasks|canvas|exec)"?/i.exec(s);
  if (!named) return null;
  const tool = named[1].toLowerCase();
  if (tool === "notes") {
    const args = noteFields(s);
    if (!args) return null;
    return { type: "tool_call", tool, args };
  }
  const q = /"(?:q|query)"\s*:\s*"([^"]*)"/.exec(s);
  const url = /"url"\s*:\s*"(https?:[^"]*)"/.exec(s);
  return {
    type: "tool_call",
    tool,
    args: { q: q ? q[1] : "", url: url ? url[1] : "" },
  };
}

// A notes call the page can run. An empty object is the model reaching for
// the tool with nothing to save, open, or list.
function fieldOf(s, key) {
  const quoted = new RegExp('"' + key + '"\\s*:\\s*"((?:\\\\.|[^"\\\\])*)"').exec(s);
  if (quoted) return quoted[1].replace(/\\n/g, "\n").replace(/\\"/g, '"').replace(/\\\\/g, "\\");
  const bare = new RegExp("\\b" + key + "\\s*[:=]\\s*\"?([^\"\\n,}]+)").exec(s);
  return bare ? bare[1].trim() : "";
}

function noteFields(s) {
  const op = fieldOf(s, "op").toLowerCase();
  const path = fieldOf(s, "path");
  const text = fieldOf(s, "text");
  if (!op && !path && !text) return null;
  const args = {};
  if (op) args.op = op;
  if (path) args.path = path;
  if (text) args.text = text;
  return args;
}

export function noteUseful(call) {
  if (!call || call.tool !== "notes") return false;
  const a = call.args || {};
  if (String(a.path || "").trim()) return true;
  if (String(a.text || "").trim()) return true;
  const op = String(a.op || "").toLowerCase();
  return op === "list" || op === "ls" || op === "read" || op === "open" || op === "write" || op === "save" || op === "rm";
}

// The person asked to use the notes kept on this device. A draft they want
// to read in the chat is not one of those.
export function wantsSavedNotes(text) {
  return /\b(save|keep|store|put)\b.{0,40}\bnotes?\b|\bmy notes\b|\b(list|show|open|read)\b.{0,24}\bnotes?\b/i.test(String(text || ""));
}

// Words already in a notes call: a sentence before the JSON, the text field,
// then a sentence after it. "..." is the prompt's placeholder, not a note.
export function noteWords(call, lead, trail) {
  const text = call && call.args ? String(call.args.text || "").trim() : "";
  const body = text === "..." || text === "…" ? "" : text;
  const parts = [String(lead || "").trim(), body, String(trail || "").trim()].filter(Boolean);
  return parts.join("\n\n");
}

// A short reply when the model answered a draft with an empty notes call.
// The person asked to read words here.
export function draftText(ask) {
  const q = String(ask || "").replace(/\s+/g, " ").trim();
  if (/\bsummar/i.test(q)) {
    return "A plan in a few sentences: name the outcome, name the next step, and name what can wait.";
  }
  if (/\b(draft|write|note|plan)\b/i.test(q)) {
    return "A short note.\n\nOne thing to do today, one thing to remember, and one thing to leave for later.";
  }
  return "Here is a reply you can read here.";
}

// The words to show for a notes-shaped reply. "" when the reply is not notes,
// so a real save, list, or other tool still runs.
export function chatNote(raw, ask) {
  const view = toolStream(raw);
  const notes = (view.call && view.call.tool === "notes") || view.tool === "notes";
  if (!notes) return "";
  const src = String(raw || "");
  const at = toolStart(src);
  let trail = "";
  if (at >= 0) {
    const rest = src.slice(at);
    const obj = rest[0] === "{" ? takeObject(rest) : "";
    if (obj) trail = cleanLead(rest.slice(obj.length));
  }
  return noteWords(view.call, view.lead, trail) || draftText(ask);
}

// The first balanced {...} in s, or "" when the brace has not closed yet.
function takeObject(s) {
  if (!s || s[0] !== "{") return "";
  let depth = 0;
  let str = false;
  let esc = false;
  for (let i = 0; i < s.length; i++) {
    const c = s[i];
    if (str) {
      if (esc) esc = false;
      else if (c === "\\") esc = true;
      else if (c === '"') str = false;
      continue;
    }
    if (c === '"') { str = true; continue; }
    if (c === "{") depth++;
    else if (c === "}") {
      depth--;
      if (depth === 0) return s.slice(0, i + 1);
    }
  }
  return "";
}

function objectsIn(text) {
  const s = String(text || "");
  const out = [];
  let str = false;
  let esc = false;
  for (let i = 0; i < s.length; i++) {
    const c = s[i];
    if (str) {
      if (esc) esc = false;
      else if (c === "\\") esc = true;
      else if (c === '"') str = false;
      continue;
    }
    if (c === '"') { str = true; continue; }
    if (c !== "{") continue;
    const obj = takeObject(s.slice(i));
    if (!obj) break;
    out.push(obj);
    i += obj.length - 1;
  }
  return out;
}

export function toolCallOf(text) {
  const direct = parseCall(text);
  if (direct) return direct;
  let raw = String(text || "").trim().replace(/^\s*champ\s*[,:]\s*/i, "");
  const opened = /^```(?:json)?\s*/i.exec(raw);
  if (opened) {
    raw = raw.slice(opened[0].length).replace(/\s*```\s*$/, "").trim();
    const inner = parseCall(raw);
    if (inner) return inner;
  }
  for (const obj of objectsIn(text)) {
    const inner = parseCall(obj);
    if (inner) return inner;
  }
  return null;
}

function cleanLead(s) {
  return String(s || "")
    .replace(/^\s*champ\s*[,:]\s*/i, "")
    .replace(/```(?:json)?[ \t]*$/i, "")
    .trim();
}

// compact form, so a pretty-printed call matches the same prefix as one line.
function squash(s) {
  return String(s || "").replace(/\s+/g, "");
}

// True when s, from a "{", is a tool call still arriving or already whole.
function opensTool(s, atStart) {
  const c = squash(s);
  if (!c.startsWith("{")) return false;
  if (c === "{") return !!atStart;
  const heads = [
    '{"type":"tool_call"',
    '{"type":"tool_result"',
    "{type:tool_call",
    '{type:"tool_call"',
  ];
  for (const h of heads) {
    const n = Math.min(c.length, h.length);
    if (c.slice(0, n) === h.slice(0, n)) return true;
  }
  return /^\{"(?:tool|name)":"[a-z]*"?/.test(c);
}

function nameSoFar(rest) {
  const m = /"(?:tool|name)"\s*:\s*"([a-z]*)/.exec(rest)
    || /\btool\s*[:=]\s*"?([a-z]*)/i.exec(rest);
  if (!m) return "";
  const frag = m[1].toLowerCase();
  if (!frag) return "";
  if (TOOL_NAMES.includes(frag)) return frag;
  const hits = TOOL_NAMES.filter((n) => n.startsWith(frag));
  return hits.length === 1 ? hits[0] : "";
}

function toolStart(raw) {
  const s = String(raw || "");
  const head = s.trim().toLowerCase();
  if (head.startsWith("tool:") && "tool:datetime".startsWith(head) && head.length >= 5) return s.search(/\S/);
  if (head === "tool:datetime") return s.toLowerCase().indexOf("tool:datetime");
  for (let i = 0; i < s.length; i++) {
    if (s[i] !== "{") continue;
    const atStart = cleanLead(s.slice(0, i)) === "";
    if (opensTool(s.slice(i), atStart)) return i;
  }
  const dt = s.toLowerCase().indexOf("tool:datetime");
  if (dt >= 0 && cleanLead(s.slice(0, dt)) === "") return dt;
  return -1;
}

// A reply as it grows.
//   text  words for the bubble
//   load  a tool call is still arriving — hide the JSON
//   call  the call is complete — hide the JSON and run it
// lead is a sentence before the call. Empty when the reply is only the call.
export function toolStream(text) {
  const raw = String(text || "");
  const bare = raw.trim().replace(/^\s*champ\s*[,:]\s*/i, "");
  if (/^```(?:json)?\s*$/i.test(bare)) return { kind: "load", call: null, lead: "", tool: "" };
  const at = toolStart(raw);
  if (at < 0) return { kind: "text", call: null, lead: raw, tool: "" };
  const lead = cleanLead(raw.slice(0, at));
  const rest = raw.slice(at);
  const timed = rest.trim();
  if (/^tool:datetime\s*$/i.test(timed)) {
    return { kind: "call", call: { type: "tool_call", tool: "datetime", args: {} }, lead, tool: "datetime" };
  }
  if ("tool:datetime".startsWith(timed.toLowerCase()) && timed.length >= 5) {
    return { kind: "load", call: null, lead, tool: "" };
  }
  const obj = rest[0] === "{" ? takeObject(rest) : "";
  if (!obj) {
    if (opensTool(rest, lead === "")) return { kind: "load", call: null, lead, tool: nameSoFar(rest) };
    return { kind: "text", call: null, lead: raw, tool: "" };
  }
  const call = parseCall(obj);
  if (call) return { kind: "call", call, lead, tool: call.tool };
  // A notes object with no path, text, or op. Hide it. The page does not list
  // an empty notebook in its place.
  if (/tool_call/.test(obj) && /\bnotes\b/i.test(obj)) {
    return { kind: "call", call: { type: "tool_call", tool: "notes", args: {} }, lead, tool: "notes" };
  }
  if (/"type"\s*:\s*"tool_result"/.test(obj)) return { kind: "load", call: null, lead, tool: nameSoFar(obj) };
  return { kind: "text", call: null, lead: raw, tool: "" };
}

export function echoedResult(text) {
  let s = (text || "").trim();
  const fence = /^```(?:json)?\s*([\s\S]*?)\s*```$/i.exec(s);
  if (fence) s = fence[1].trim();
  if (!s.startsWith("{") || !s.endsWith("}")) return "";
  try {
    const obj = JSON.parse(s);
    if (obj && obj.type === "tool_result" && typeof obj.output === "string") return obj.output;
  } catch { return ""; }
  return "";
}

const LOOKUP = /\b(look\s*up|lookup|search(?:\s+for)?|weather|forecast)\b/i;

export function wantsLookup(text) {
  return LOOKUP.test(String(text || ""));
}

export function lookupQuery(text) {
  const raw = String(text || "").trim();
  const q = raw.replace(/^\s*(please\s+)?(look\s*up|lookup|search(?:\s+for)?)\s+/i, "").trim();
  return (q || raw).slice(0, 180);
}

const TEACH = /\b(?:teach|study|learn\s+(?:about|how|the|this|that|why|what|from)|foundational model)\b/i;

export function wantsTeach(text) {
  return TEACH.test(String(text || ""));
}

export function teachQuery(text) {
  let q = String(text || "").trim();
  q = q.replace(/^\s*(please\s+)?/i, "");
  q = q.replace(/^(?:ask\s+(?:a\s+)?foundational model\s+to\s+)?/i, "");
  q = q.replace(/^(?:teach\s+(?:yourself|me|champ|you)\s+|teach\s+|study\s+|learn\s+)/i, "");
  q = q.replace(/^(?:about|from)\s+/i, "");
  q = q.trim();
  return (q || String(text || "").trim()).slice(0, 180);
}

// The web addresses in text, as the page compares them (URL href), with trailing punctuation off.
export function urlsIn(text) {
  const out = [];
  for (const m of String(text || "").matchAll(/https?:\/\/[^\s<>"'`)\]]+/gi)) {
    const raw = m[0].replace(/[.,;:!?]+$/, "");
    try { out.push(new URL(raw).href); } catch { /* not an address */ }
  }
  return out;
}

export function unknownReply(text) {
  const s = String(text || "").replace(/\s+/g, " ").trim();
  if (!s) return false;
  if (/\b(?:don'?t|do not) understand\b/i.test(s)) return true;
  if (/\bcould you (?:please )?clarify\b/i.test(s)) return true;
  if (/\bnot sure what you mean\b/i.test(s)) return true;
  if (/\b(?:don'?t|do not) know what (?:you|that|this)\b/i.test(s)) return true;
  if (/\b(?:i am|i'm) not familiar\b/i.test(s)) return true;
  if (/\b(?:don'?t|do not) have (?:enough |any )?(?:real-time |current )?(?:information|data|access|capabilities)\b/i.test(s)) return true;
  if (/\b(?:unable to|can'?t|cannot) (?:answer|help with|understand)\b/i.test(s)) return true;
  if (/^(?:i'?m |i am )?sorry[,.]?\s+(?:but )?i (?:don'?t|do not) know\b/i.test(s) && s.length < 320) return true;
  return false;
}

export function acceptsLearn(text) {
  return /^(?:yes(?:\s+please)?|yeah|yep|yup|sure|please|ok|okay|go ahead|learn it|learn this|do it)[.!?\s]*$/i.test(String(text || "").trim());
}

export function declinesLearn(text) {
  return /^(?:no(?:\s+thanks)?|nope|nah|no thank you)[.!?\s]*$/i.test(String(text || "").trim());
}

// A tool runs when the person asked for it or picked it. Datetime is the clock.
// Anything else is the model inventing a call, and the page does not run it.
export function callAllowed(call, ask, picks) {
  if (!call || !call.tool) return false;
  if (call.tool === "datetime") return true;
  return toolsFor(ask, picks).includes(call.tool);
}

// The model described a tool instead of calling one, or handed back an empty id.
export function playedTool(text) {
  const s = String(text || "").trim();
  if (!s || toolCallOf(s)) return false;
  if (/completed by using\b[\s\S]{0,80}\b(function|tool|canvas|notes|tasks)\b/i.test(s)) return true;
  if (/the response was:\s*\{/i.test(s)) return true;
  if (/^\s*\{\s*"id"\s*:\s*null\s*\}\s*$/.test(s)) return true;
  return false;
}

export function toolStatus(tool) {
  if (tool === "search") return "Searching…";
  if (tool === "gather") return "Reading…";
  if (tool === "datetime") return "Checking the time…";
  if (tool === "notes") return "Opening notes…";
  if (tool === "tasks") return "Checking tasks…";
  if (tool === "canvas") return "Drawing…";
  if (tool === "exec") return "Running…";
  return "Working…";
}

function opOf(call) {
  return String((call.args && call.args.op) || "").toLowerCase().trim();
}

// The page's own notes, tasks and canvas, opened on first use.
let services = null;
function servicesOf(ctx) {
  if (ctx && ctx.services) return ctx.services;
  if (!services) services = createServices();
  return services;
}

async function localTool(svc, tool, args) {
  const j = await svc.run(tool, args);
  if (!j || typeof j.output !== "string") return { output: "The " + tool + " tool failed.", exit_code: 1 };
  return { output: j.output, exit_code: j.ok ? 0 : 1, data: j.data || null };
}

// ctx: { vm, confirm(command) -> Promise<bool> } for exec; services for notes, tasks and canvas.
async function runAgent(call, ctx) {
  const args = Object.assign({}, call.args || {});
  if (call.tool === "exec") {
    const cmd = String(args.cmd || args.command || args.q || "").trim();
    if (!cmd) return { output: "Missing command.", exit_code: 1 };
    if (!ctx || !ctx.vm) return { output: "Running code needs EncryptidVM on this machine.", exit_code: 1 };
    if (ctx.confirm && !(await ctx.confirm(cmd))) return { output: "Not run: the person declined.", exit_code: 1, declined: true };
    const r = await ctx.vm.exec(cmd);
    return Object.assign({ data: { command: cmd } }, r);
  }
  if (call.tool === "tasks" && !args.op && args.title) args.op = "add";
  if (call.tool === "notes" && !args.op) args.op = args.text != null ? "write" : (args.path ? "read" : "list");
  if (call.tool === "canvas" && !args.op) args.op = args.text != null ? "write" : (args.title ? "read" : "list");
  try { return await localTool(servicesOf(ctx), call.tool, args); }
  catch { return { output: "The " + call.tool + " tool failed.", exit_code: 1 }; }
}

async function readTool(fetchImpl, path, params) {
  const u = new URL(path, "http://champ.local");
  for (const [k, v] of Object.entries(params)) u.searchParams.set(k, v);
  const r = await fetchImpl(u.pathname + u.search, { headers: { Accept: "application/json" } });
  const j = await r.json().catch(() => null);
  if (!r.ok || !j || typeof j.output !== "string") return { output: "The lookup failed.", exit_code: 1 };
  return { output: j.output, exit_code: j.ok ? 0 : 1 };
}

export async function runTool(call, fetchImpl, ctx) {
  const f = fetchImpl || fetch;
  if (!call || !TOOLS.has(call.tool)) return { output: "Unknown tool.", exit_code: 1 };
  if (call.tool === "datetime") return { output: clockText(), exit_code: 0 };
  if (AGENT.has(call.tool)) return runAgent(call, ctx);
  if (call.tool === "search") {
    const q = String((call.args && (call.args.q || call.args.query)) || "").trim();
    if (!q) return { output: "Missing search query.", exit_code: 1 };
    try { return await readTool(f, "/api/ext/search", { q }); }
    catch { return { output: "The lookup failed.", exit_code: 1 }; }
  }
  const url = String((call.args && call.args.url) || "").trim();
  if (!url) return { output: "Missing address.", exit_code: 1 };
  // gather reads an address the person wrote or a search listed, never one the model made up
  // (or copied from an example).
  let href = "";
  try { href = new URL(url).href; } catch { href = ""; }
  if (ctx && typeof ctx.known === "function" && (!href || !ctx.known(href))) {
    return { output: "Hodge-3.5 reads only an address the person gave or a search listed.", exit_code: 1 };
  }
  try { return await readTool(f, "/api/ext/gather", { url }); }
  catch { return { output: "The lookup failed.", exit_code: 1 }; }
}

// Decides what the page does when a reply finishes.
// finish: show the text. follow: run a tool, then send `wire` as the next turn.
// The caller must leave `fallback` on screen if the next turn cannot start.
// An agent tool's raw result comes back as `card` for the page to show
// beside Champ's words; a write is shown as is and ends the turn.
export async function settleReply(raw, rounds, run) {
  const echo = echoedResult(raw);
  if (echo) return { action: "show", text: echo, rounds: 0 };
  const call = rounds < MAX_ROUNDS ? toolCallOf(raw) : null;
  if (!call) return { action: "finish", rounds: 0 };
  if (rounds >= 2 && !AGENT.has(call.tool)) return { action: "finish", rounds: 0 };
  const ran = await run(call);
  if (call.tool === "datetime" || !TOOLS.has(call.tool)) {
    return { action: "show", text: ran.output, rounds: 0 };
  }
  if (AGENT.has(call.tool)) {
    const card = { tool: call.tool, args: call.args || {}, output: ran.output, exit_code: ran.exit_code, data: ran.data || null };
    if (ran.declined || (call.tool !== "exec" && WRITES.has(opOf(call)))) {
      return { action: "show", text: ran.output, card, rounds: 0 };
    }
    return {
      action: "follow",
      status: toolStatus(call.tool),
      fallback: ran.output,
      card,
      wire: "Results:\n" + ran.output + "\nAnswer the person in plain words, or reply with one more tool call if the task needs it.",
      rounds: rounds + 1,
    };
  }
  const wire = "Results:\n" + ran.output + "\nAnswer the person in plain words. Do not use JSON.";
  return {
    action: "follow",
    status: toolStatus(call.tool),
    fallback: ran.output,
    wire,
    rounds: rounds + 1,
  };
}

// The composer's + menu: tools the person picks for the next messages.
// Each pick is told to the model with the one JSON form it should reply with.
export const PICKS = [
  { tool: "search", label: "Search the web", form: '{"type":"tool_call","tool":"search","args":{"q":"query"}}' },
  { tool: "gather", label: "Read a page", form: '{"type":"tool_call","tool":"gather","args":{"url":"ADDRESS"}}' },
  { tool: "notes", label: "Notes", form: '{"type":"tool_call","tool":"notes","args":{"op":"write","path":"plans/today.md","text":"..."}} (op is write, read, list or rm)' },
  { tool: "tasks", label: "Tasks", form: '{"type":"tool_call","tool":"tasks","args":{"op":"add","title":"..."}} (op is add, list, update, done, start or comment, with id for update, done, start and comment)' },
  { tool: "canvas", label: "Canvas", form: '{"type":"tool_call","tool":"canvas","args":{"op":"write","title":"...","text":"markdown"}} (op is write, read or list)' },
  { tool: "exec", label: "Run code", form: '{"type":"tool_call","tool":"exec","args":{"cmd":"ls"}}' },
];

// The line put before the person's message when they picked tools, "" for none.
export function pickedHint(tools) {
  const picked = PICKS.filter((p) => (tools || []).includes(p.tool));
  if (!picked.length) return "";
  if (picked.length === 1) {
    const p = picked[0];
    return "The person chose the " + p.tool + " tool for this. Reply with only " + p.form + ".";
  }
  return "The person chose these tools for this: " + picked.map((p) => p.tool).join(", ") +
    ". Reply with only one tool call at a time, in its form:\n" + picked.map((p) => p.form).join("\n");
}
