// Champ's notes, tasks and canvas, in this browser. The same tools the site
// ran over GitMesh, TodoTraq and MotionKB, with the same ops and answers,
// kept in this browser's IndexedDB ("champ-svc"). No sign-in, and nothing
// leaves the page. Without IndexedDB (private window, blocked storage) they
// work in memory for this visit.
//
//	notes   {op: list|read|write|rm, path, text, message}          GitMesh: champ-notes, one commit per change
//	tasks   {op: add|list|update|done|start|comment, title, body, id, state, text}   TodoTraq: project CHAMP, Task type
//	canvas  {op: write|read|list, title, text}                     MotionKB: site "Champ Canvas", markdown as blocks
//
// Answers are {ok, output, data}: output is what Champ reads, data feeds the page.

const DB = "champ-svc";
const STORES = { files: "path", commits: "seq", tickets: "number", comments: "seq" };

const CANVAS_STATE = ".champ/canvas.json";
const MAX_NOTE = 256 << 10;
const MAX_PATH = 200;
const MAX_LIST = 300;

// TodoTraq's default Task type (0trustcloud work/types_default.go).
const TASK_TYPE = { id: "task", name: "Task", states: ["backlog", "open", "in_progress", "blocked", "done"] };
const PROJECT_KEY = "CHAMP";

// ---- storage ----

function memoryBackend() {
  const stores = {};
  for (const s of Object.keys(STORES)) stores[s] = new Map();
  return {
    async get(store, key) { return stores[store].get(key) || null; },
    async all(store) { return [...stores[store].values()]; },
    // ops: [{store, put: rec} | {store, del: key}], all or none.
    async apply(ops) {
      for (const o of ops) {
        const m = stores[o.store];
        if ("del" in o) m.delete(o.del);
        else m.set(o.put[STORES[o.store]], structuredClone(o.put));
      }
    },
  };
}

function openDB() {
  return new Promise((resolve) => {
    let req;
    try { req = indexedDB.open(DB, 1); } catch { resolve(null); return; }
    req.onupgradeneeded = () => {
      for (const [name, keyPath] of Object.entries(STORES)) req.result.createObjectStore(name, { keyPath });
    };
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => resolve(null);
    req.onblocked = () => resolve(null);
  });
}

function idbBackend(db) {
  const done = (req) => new Promise((resolve, reject) => {
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => reject(req.error);
  });
  return {
    async get(store, key) { return (await done(db.transaction(store, "readonly").objectStore(store).get(key))) || null; },
    async all(store) { return (await done(db.transaction(store, "readonly").objectStore(store).getAll())) || []; },
    apply(ops) {
      return new Promise((resolve, reject) => {
        const tx = db.transaction([...new Set(ops.map((o) => o.store))], "readwrite");
        for (const o of ops) {
          const s = tx.objectStore(o.store);
          if ("del" in o) s.delete(o.del);
          else s.put(o.put);
        }
        tx.oncomplete = () => resolve();
        tx.onerror = () => reject(tx.error);
        tx.onabort = () => reject(tx.error || new Error("aborted"));
      });
    },
  };
}

async function defaultBackend() {
  if (typeof indexedDB === "undefined") return memoryBackend();
  const db = await openDB();
  return db ? idbBackend(db) : memoryBackend();
}

// ---- helpers ----

const enc = new TextEncoder();
const byteLen = (s) => enc.encode(String(s)).length;

function ok(output, data) {
  const r = { ok: true, output };
  if (data) r.data = data;
  return r;
}

function fail(output) {
  return { ok: false, output };
}

function firstSet(...v) {
  for (const s of v) if (s) return s;
  return "";
}

function errText(e) {
  return String((e && e.message) || e || "error");
}

async function shaHex(text) {
  try {
    const d = await crypto.subtle.digest("SHA-1", enc.encode(text));
    return [...new Uint8Array(d)].map((b) => b.toString(16).padStart(2, "0")).join("");
  } catch {
    let h = "";
    for (let i = 0; i < 40; i++) h += Math.floor(Math.random() * 16).toString(16);
    return h;
  }
}

const now = () => new Date().toISOString();

// ---- notes: GitMesh ----

export function cleanNotePath(p) {
  let s = String(p || "").replace(/\\/g, "/").trim().replace(/^\/+/, "");
  while (s.includes("//")) s = s.replace(/\/\//g, "/");
  if (!s || s.length > MAX_PATH || s.endsWith("/")) return "";
  for (const part of s.split("/")) {
    if (!part || part === "." || part === ".." || part.toLowerCase() === ".git") return "";
    for (const ch of part) {
      if (ch.charCodeAt(0) < 32 || '<>:"|?*'.includes(ch)) return "";
    }
  }
  return s;
}

// ---- tasks: TodoTraq ----

// Champ says todo / doing / done; TodoTraq's Task type has its own states.
export function taskState(s, states) {
  let v = String(s || "").trim().toLowerCase();
  if (["todo", "to do", "open", "new"].includes(v)) v = "open";
  else if (["doing", "start", "started", "in progress", "progress", "wip"].includes(v)) v = "in_progress";
  else if (["done", "close", "closed", "finished", "complete", "completed"].includes(v)) v = "done";
  return (states || TASK_TYPE.states).includes(v) ? v : "";
}

function ticketLine(t) {
  return t.key + " [" + t.state + "] " + t.title;
}

export function findTicket(ts, ref) {
  const raw = String(ref || "");
  const r = raw.replace(/^#/, "").trim().toUpperCase();
  const m = /(\d+)$/.exec(r);
  for (const t of ts) {
    if (t.key === r || t.id === raw) return t;
    if (m && String(t.number) === m[1] && (r === m[1] || r.startsWith(PROJECT_KEY))) return t;
  }
  return null;
}

// ---- canvas: MotionKB ----

// markdownBlocks turns Champ's markdown into MotionKB blocks.
export function markdownBlocks(md) {
  const out = [];
  let para = [];
  const flush = () => {
    if (para.length) {
      out.push({ type: "paragraph", content: para.join(" ") });
      para = [];
    }
  };
  const lines = String(md || "").replace(/\r\n/g, "\n").split("\n");
  for (let i = 0; i < lines.length; i++) {
    const t = lines[i].replace(/[ \t]+$/, "").trim();
    if (t.startsWith("```")) {
      flush();
      const code = [];
      for (i++; i < lines.length && !lines[i].trim().startsWith("```"); i++) code.push(lines[i]);
      out.push({ type: "code", content: code.join("\n") });
      continue;
    }
    let m;
    if (t === "") flush();
    else if ((m = /^(#{1,3})\s+(.*)$/.exec(t))) { flush(); out.push({ type: "heading" + m[1].length, content: m[2] }); }
    else if ((m = /^[-*]\s+\[([ xX])\]\s+(.*)$/.exec(t))) { flush(); out.push({ type: "todo", content: m[2], checked: m[1] !== " " }); }
    else if ((m = /^(?:[-*+]|\d+[.)])\s+(.*)$/.exec(t))) { flush(); out.push({ type: "bullet", content: m[1] }); }
    else if (t.startsWith(">")) { flush(); out.push({ type: "quote", content: t.slice(1).trim() }); }
    else para.push(t);
  }
  flush();
  return out;
}

export function slugOf(title) {
  let s = String(title || "").toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-+|-+$/g, "");
  if (s.length > 60) s = s.slice(0, 60).replace(/^-+|-+$/g, "");
  return s || "canvas";
}

// ---- the services ----

export function createServices(opts) {
  const o = opts || {};
  let backendP = null;
  const backend = () => backendP || (backendP = o.backend ? Promise.resolve(o.backend) : defaultBackend());
  // One change at a time, so a task number or a commit's parent is never taken twice.
  let chain = Promise.resolve();
  const serial = (fn) => {
    const run = chain.then(fn, fn);
    chain = run.catch(() => {});
    return run;
  };

  async function readNote(path) {
    const f = await (await backend()).get("files", path);
    return f ? f.content : null;
  }

  async function commitNotes(message, files, deletes) {
    const b = await backend();
    const log = await b.all("commits");
    const parent = log.reduce((a, c) => (c.seq > a.seq ? c : a), { seq: 0, id: "" });
    const at = now();
    const id = await shaHex(JSON.stringify({ parent: parent.id, message, files, deletes, at }));
    const ops = [];
    for (const [path, content] of Object.entries(files || {})) ops.push({ store: "files", put: { path, content, commit: id, updated: at } });
    for (const path of deletes || []) ops.push({ store: "files", del: path });
    ops.push({ store: "commits", put: { seq: parent.seq + 1, id, parent: parent.id, message, at, paths: Object.keys(files || {}), deletes: deletes || [] } });
    await b.apply(ops);
    return id;
  }

  async function notes(a) {
    switch (a.op) {
      case "": case "list": case "ls": {
        const pre = String(a.path || "").trim().replace(/^\/+/, "");
        const names = (await (await backend()).all("files"))
          .map((f) => f.path)
          .filter((p) => !p.startsWith(".champ/") && p.startsWith(pre))
          .sort()
          .slice(0, MAX_LIST);
        if (!names.length) return ok("No notes yet.", { files: [] });
        return ok(names.join("\n"), { files: names });
      }
      case "read": case "open": case "cat": {
        const p = cleanNotePath(a.path);
        if (!p) return fail("Which note? Give a path such as plans/today.md.");
        const text = await readNote(p);
        if (text == null) return fail("There is no note " + p + ".");
        return ok(text, { path: p, text });
      }
      case "write": case "save": case "put": {
        const p = cleanNotePath(a.path);
        if (!p) return fail("A note needs a path such as plans/today.md.");
        const text = String(a.text == null ? "" : a.text);
        if (byteLen(text) > MAX_NOTE) return fail("That note is over " + MAX_NOTE + " bytes.");
        const sha = await serial(() => commitNotes(firstSet(String(a.message || "").trim(), "champ: write " + p), { [p]: text }, []));
        return ok("Saved " + p + ".", { path: p, commit: sha });
      }
      case "rm": case "remove": case "delete": {
        const p = cleanNotePath(a.path);
        if (!p) return fail("Which note?");
        if ((await readNote(p)) == null) return fail("There is no note " + p + ".");
        await serial(() => commitNotes("champ: remove " + p, {}, [p]));
        return ok("Removed " + p + ".", { path: p });
      }
    }
    return fail("Notes can list, read, write or rm.");
  }

  async function listTickets() {
    return (await (await backend()).all("tickets")).sort((x, y) => x.number - y.number);
  }

  async function tasks(a) {
    const states = TASK_TYPE.states;
    switch (a.op) {
      case "": case "list": case "ls": {
        let want = "";
        if (a.state) {
          want = taskState(a.state, states);
          if (!want) return fail("State is one of " + states.join(", ") + ".");
        }
        const shown = (await listTickets()).filter((t) => t.typeId === TASK_TYPE.id && (!want || t.state === want));
        if (!shown.length) return ok("No tasks.", { tasks: [] });
        return ok(shown.map(ticketLine).join("\n"), { tasks: shown });
      }
      case "add": case "new": case "create": {
        let title = firstSet(a.title, a.text).trim();
        if (!title) return fail("A task needs a title.");
        if (title.length > 200) title = title.slice(0, 200);
        const t = await serial(async () => {
          const n = (await listTickets()).reduce((m, x) => Math.max(m, x.number), 0) + 1;
          const at = now();
          const rec = {
            id: PROJECT_KEY + "-" + n, number: n, key: PROJECT_KEY + "-" + n, typeId: TASK_TYPE.id,
            title, state: taskState("open", states), fields: { summary: String(a.body || "") }, created: at, updated: at,
          };
          await (await backend()).apply([{ store: "tickets", put: rec }]);
          return rec;
        });
        return ok("Added " + ticketLine(t) + ".", { task: t, tasks: [t] });
      }
      case "update": case "set": case "done": case "start": case "close": case "comment": {
        const t = findTicket(await listTickets(), a.id);
        if (!t) return fail("There is no task " + (a.id || "") + ".");
        if (a.op === "comment") {
          const text = firstSet(a.text, a.body).trim();
          if (!text) return fail("A comment needs text.");
          await serial(async () => {
            const b = await backend();
            const seq = (await b.all("comments")).reduce((m, c) => Math.max(m, c.seq), 0) + 1;
            await b.apply([{ store: "comments", put: { seq, ticket: t.number, content: text, created: now() } }]);
          });
          return ok("Commented on " + t.key + ".", { task: t });
        }
        let state = a.state || "";
        if (a.op === "done" || a.op === "close") state = "done";
        else if (a.op === "start") state = "doing";
        const nt = Object.assign({}, t);
        let changed = false;
        if (state) {
          const s = taskState(state, states);
          if (!s) return fail("State is one of " + states.join(", ") + ".");
          nt.state = s;
          changed = true;
        }
        if (String(a.title || "").trim()) {
          nt.title = String(a.title).trim().slice(0, 200);
          changed = true;
        }
        if (!changed) return fail("Nothing to change on " + t.key + ".");
        nt.updated = now();
        await serial(async () => (await backend()).apply([{ store: "tickets", put: nt }]));
        return ok("Updated " + ticketLine(nt) + ".", { task: nt, tasks: [nt] });
      }
    }
    return fail("Tasks can add, list, update, done, start or comment.");
  }

  async function loadCanvas() {
    const raw = await readNote(CANVAS_STATE);
    let d = null;
    try { d = raw ? JSON.parse(raw) : null; } catch { d = null; }
    if (!d || typeof d !== "object") d = {};
    if (!d.site_id) d.site_id = "champ-canvas";
    if (!d.pages || typeof d.pages !== "object") d.pages = {};
    return d;
  }

  async function canvas(a) {
    switch (a.op) {
      case "list": case "ls": {
        const titles = Object.values((await loadCanvas()).pages).map((p) => p.title).sort();
        if (!titles.length) return ok("No canvases yet.");
        return ok(titles.join("\n"), { titles });
      }
      case "read": case "open": case "show": {
        const slug = slugOf(a.title);
        const p = (await loadCanvas()).pages[slug];
        const text = await readNote("canvas/" + slug + ".md");
        if (text == null || !p) return fail("There is no canvas " + JSON.stringify(String(a.title || "")) + ".");
        return ok(text, { title: p.title, text, blocks: p.blocks || [] });
      }
      case "": case "write": case "draw": case "set": {
        const title = String(a.title || "").trim() || "Canvas";
        const text = String(a.text == null ? "" : a.text);
        if (byteLen(text) > MAX_NOTE) return fail("That canvas is over " + MAX_NOTE + " bytes.");
        const blocks = markdownBlocks(text);
        if (!blocks.length) return fail("The canvas needs some markdown to show.");
        const slug = slugOf(title);
        await serial(async () => {
          const d = await loadCanvas();
          const p = d.pages[slug] || { title, page_id: slug };
          p.blocks = blocks.map((b, i) => Object.assign({ position: i }, b));
          d.pages[slug] = p;
          await commitNotes("champ: canvas " + title, { ["canvas/" + slug + ".md"]: text, [CANVAS_STATE]: JSON.stringify(d, null, 2) }, []);
        });
        return ok("Canvas " + JSON.stringify(title) + " has " + blocks.length + " blocks.", { title, text, blocks });
      }
    }
    return fail("Canvas can write, read or list.");
  }

  const tools = { notes, tasks, canvas };
  return {
    // run(tool, args) -> {ok, output, data}
    async run(tool, args) {
      const fn = tools[tool];
      if (!fn) return fail("Unknown tool.");
      const a = Object.assign({}, args || {});
      a.op = String(a.op || "").trim().toLowerCase();
      try { return await fn(a); }
      catch (e) { return fail("The " + tool + " tool failed: " + errText(e)); }
    },
    // A note's commit log, newest first.
    async log() {
      return (await (await backend()).all("commits")).sort((x, y) => y.seq - x.seq);
    },
    async comments(ref) {
      const t = findTicket(await listTickets(), ref);
      if (!t) return [];
      return (await (await backend()).all("comments")).filter((c) => c.ticket === t.number).sort((x, y) => x.seq - y.seq);
    },
  };
}

export { memoryBackend };
