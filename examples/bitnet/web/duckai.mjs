// duck.ai answers only when its own page sends the chat.
// Chrome stays headless. Page commands are Runtime.evaluate,
// the same LocalFrame.JavaScriptExecuteRequest Bruja already runs.
import { spawn } from "node:child_process";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

const PORT = 9224;
const ROOT = "http://127.0.0.1:" + PORT;
const PROFILE = path.join(os.tmpdir(), "champ-duckai-headless");
const PAGE_UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/155.0.0.0 Safari/537.36";

const SPOT = `(() => {
  const ta = document.querySelector("textarea");
  const btn = [...document.querySelectorAll("button")].find((b) => {
    const a = b.getAttribute("aria-label") || "";
    const t = (b.innerText || "").replace(/\\s+/g, " ").trim();
    return a === "Send" || a === "Ask" || t === "Ask" || t === "Send";
  });
  if (!ta || !btn) return { len: ta ? ta.value.length : -1, disabled: true, w: 0, h: 0, x: 0, y: 0 };
  btn.scrollIntoView({ block: "center" });
  const box = btn.getBoundingClientRect();
  return { len: ta.value.length, disabled: !!btn.disabled, x: box.x + box.width / 2, y: box.y + box.height / 2, w: box.width, h: box.height };
})()`;

const VIEW = `(() => {
  const t = (document.body.innerText || "").replace(/\\s+/g, " ").trim();
  const at = t.toLowerCase().indexOf("you said");
  const stop = [...document.querySelectorAll("button")].some((b) => b.getAttribute("aria-label") === "Stop generating" && !b.disabled);
  const loc = [...document.querySelectorAll("button")].find((b) => (b.innerText || "").trim() === "Don't Use");
  let where = null;
  if (loc) {
    loc.scrollIntoView({ block: "center" });
    const box = loc.getBoundingClientRect();
    where = { x: box.x + box.width / 2, y: box.y + box.height / 2, w: box.width, h: box.height };
  }
  return { stop, tail: at >= 0 ? t.slice(at, at + 1600) : "", where };
})()`;

function sleep(ms) {
  return new Promise((r) => setTimeout(r, ms));
}

function note(msg) {
  if (process.env.CHAMP_DUCK_DEBUG === "1") console.error(msg);
}

export function duckAnswer(text) {
  let s = String(text || "").replace(/\s+/g, " ").trim();
  if (!s || /Searching the web|Generating response|Before continuing|Use your approximate location/i.test(s)) return "";
  const at = s.search(/Duck\.ai said/i);
  if (at < 0) return "";
  s = s.slice(at).replace(/^Duck\.ai said\s+/i, "");
  const end = s.search(/Duck\.ai works best|Jump to latest|All chats are private|AI can make mistakes/i);
  if (end > 0) s = s.slice(0, end);
  s = s.replace(/\s*Read More\b[\s\S]*$/i, "");
  s = s.replace(/\s*<\s*Citation[\s\S]*$/i, "");
  s = s.replace(/\s*Related Searches\b[\s\S]*$/i, "");
  s = s.replace(/\s*2nd opinion\b[\s\S]*$/i, "");
  s = s.replace(/^(?:[A-Za-z]+-\d[\w.-]*|\d+(?:\.\d+)*)\s+(?:[A-Za-z]{2,16}\s+)?/, "");
  s = s.replace(/\s+/g, " ").trim();
  if (s.length < 12) return "";
  return s.slice(0, 700);
}

function chromePath() {
  const candidates = [
    process.env.CHROME_PATH,
    "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
    "C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe",
    path.join(os.homedir(), "AppData", "Local", "Google", "Chrome", "Application", "chrome.exe"),
    "/usr/bin/google-chrome-stable",
    "/usr/bin/google-chrome",
    "/usr/bin/chromium",
    "/usr/bin/chromium-browser",
  ].filter(Boolean);
  return candidates.find((p) => fs.existsSync(p)) || "";
}

async function portUp() {
  try {
    const r = await fetch(ROOT + "/json/version", { signal: AbortSignal.timeout(1500) });
    return r.ok;
  } catch {
    return false;
  }
}

let launching = null;
async function ensureChrome() {
  if (await portUp()) return;
  if (!launching) {
    launching = (async () => {
      const exe = chromePath();
      if (!exe) {
        note("chrome missing");
        throw new Error("chrome missing");
      }
      fs.mkdirSync(PROFILE, { recursive: true });
      const args = [
        "--headless=new",
        "--remote-debugging-address=127.0.0.1",
        "--remote-debugging-port=" + PORT,
        "--remote-allow-origins=*",
        "--user-data-dir=" + PROFILE,
        "--user-agent=" + PAGE_UA,
        "--no-first-run",
        "--no-default-browser-check",
        "--window-size=1100,800",
        "about:blank",
      ];
      if (process.platform !== "win32") {
        args.splice(1, 0, "--no-sandbox", "--disable-gpu", "--disable-dev-shm-usage");
      }
      const child = spawn(exe, args, { detached: true, stdio: "ignore" });
      child.unref();
      for (let i = 0; i < 40; i++) {
        if (await portUp()) return;
        await sleep(250);
      }
      note("chrome did not start");
      throw new Error("chrome did not start");
    })();
  }
  try {
    await launching;
  } finally {
    launching = null;
  }
}

function connect(url) {
  const ws = new WebSocket(url);
  let id = 0;
  const pending = new Map();
  const ready = new Promise((resolve, reject) => {
    ws.addEventListener("open", resolve);
    ws.addEventListener("error", () => reject(new Error("cdp socket")));
  });
  ws.addEventListener("message", (ev) => {
    let msg;
    try { msg = JSON.parse(ev.data); } catch { return; }
    if (!msg.id || !pending.has(msg.id)) return;
    const p = pending.get(msg.id);
    pending.delete(msg.id);
    if (msg.error) p.reject(new Error(msg.error.message || "cdp"));
    else p.resolve(msg.result || {});
  });
  return {
    ws,
    send(method, params = {}, sessionId) {
      const msg = { id: ++id, method, params };
      if (sessionId) msg.sessionId = sessionId;
      return ready.then(() => new Promise((resolve, reject) => {
        pending.set(msg.id, { resolve, reject });
        ws.send(JSON.stringify(msg));
        setTimeout(() => {
          if (!pending.has(msg.id)) return;
          pending.delete(msg.id);
          reject(new Error("timeout " + method));
        }, 20000);
      }));
    },
  };
}

async function pageValue(cdp, sessionId, expression) {
  const r = await cdp.send("Runtime.evaluate", {
    expression,
    returnByValue: true,
  }, sessionId);
  if (r.exceptionDetails) return null;
  return r.result ? r.result.value : null;
}

async function clickAt(cdp, sessionId, x, y) {
  await cdp.send("Input.dispatchMouseEvent", {
    type: "mousePressed", x, y, button: "left", clickCount: 1,
  }, sessionId);
  await cdp.send("Input.dispatchMouseEvent", {
    type: "mouseReleased", x, y, button: "left", clickCount: 1,
  }, sessionId);
}

async function askOnce(question) {
  let cdp = null;
  let targetId = "";
  try {
    await ensureChrome();
    const ver = await (await fetch(ROOT + "/json/version")).json();
    cdp = connect(ver.webSocketDebuggerUrl);
    const created = await cdp.send("Target.createTarget", { url: "about:blank" });
    targetId = created.targetId || "";
    const attached = await cdp.send("Target.attachToTarget", { targetId, flatten: true });
    const sessionId = attached.sessionId;
    await cdp.send("Runtime.enable", {}, sessionId);
    await cdp.send("Page.enable", {}, sessionId);
    await cdp.send("Emulation.setUserAgentOverride", {
      userAgent: PAGE_UA,
      acceptLanguage: "en-US,en",
      platform: "Win32",
    }, sessionId);
    await cdp.send("Page.navigate", { url: "https://duck.ai/" }, sessionId);
    const born = Date.now();
    let ready = false;
    while (Date.now() - born < 15000) {
      if (await pageValue(cdp, sessionId, `!!document.querySelector("textarea")`)) {
        ready = true;
        break;
      }
      await sleep(400);
    }
    if (!ready) {
      note("no textarea");
      return "";
    }
    await cdp.send("Runtime.evaluate", {
      expression: `document.querySelector("textarea").focus()`,
      userGesture: true,
    }, sessionId);
    await cdp.send("Input.insertText", { text: question }, sessionId);
    let spot = await pageValue(cdp, sessionId, SPOT);
    if (!spot || spot.len < Math.min(8, question.length)) {
      for (const ch of question) {
        await cdp.send("Input.dispatchKeyEvent", {
          type: "keyDown", key: ch, text: ch, unmodifiedText: ch,
        }, sessionId);
        await cdp.send("Input.dispatchKeyEvent", { type: "keyUp", key: ch }, sessionId);
      }
      spot = await pageValue(cdp, sessionId, SPOT);
    }
    if (!spot || spot.disabled || !(spot.w > 2) || !(spot.h > 2)) {
      note("send not ready");
      return "";
    }
    await clickAt(cdp, sessionId, spot.x, spot.y);
    let declined = false;
    const until = Date.now() + 45000;
    while (Date.now() < until) {
      await sleep(1000);
      const view = await pageValue(cdp, sessionId, VIEW);
      if (!view) continue;
      if (view.where && !declined && view.where.w > 2) {
        declined = true;
        await clickAt(cdp, sessionId, view.where.x, view.where.y);
        continue;
      }
      if (view.stop) continue;
      const answer = duckAnswer(view.tail || "");
      if (answer) return answer;
    }
    note("empty answer");
    return "";
  } catch (err) {
    note(err && err.message ? err.message : "ask failed");
    return "";
  } finally {
    try {
      if (cdp && targetId) await cdp.send("Target.closeTarget", { targetId });
    } catch { /* tab already gone */ }
    try { if (cdp) cdp.ws.close(); } catch { /* closed */ }
  }
}

let tail = Promise.resolve();
export function askDuck(question) {
  const q = String(question || "").replace(/\s+/g, " ").trim().slice(0, 180);
  if (!q) return Promise.resolve("");
  const job = tail.then(() => askOnce(q));
  tail = job.then(() => {}, () => {});
  return job;
}
