// Champ's desk: what the tools show on the page. Tool cards in the thread
// and the "run this?" confirm for exec.
// Every text the model or a service returns is set with textContent.

import { renderMarkdown } from "./markdown.js";

const TITLES = { notes: "Notes", tasks: "Tasks", canvas: "Canvas", exec: "EncryptidVM" };

function el(tag, cls, text) {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (text != null) e.textContent = text;
  return e;
}

function head(card) {
  const a = card.args || {};
  const op = String(a.op || "").toLowerCase();
  if (card.tool === "exec") return (card.data && card.data.command) || a.cmd || a.command || "";
  if (card.tool === "notes") return [op || "list", a.path || ""].filter(Boolean).join(" ");
  if (card.tool === "tasks") return [op || "list", a.title || a.id || a.state || ""].filter(Boolean).join(" ");
  if (card.tool === "canvas") return [op || "write", a.title || ""].filter(Boolean).join(" ");
  return op;
}

// A tool's result as one card. Placed before `before` (Champ's answer) or at the end.
export function renderCard(thread, card, before) {
  const box = el("div", "tool" + (card.exit_code ? " bad" : ""));
  const top = el("div", "tool-top");
  top.append(el("b", "", TITLES[card.tool] || card.tool), el("span", "tool-what", head(card)));
  box.append(top);
  const d = card.data || {};
  if (card.tool === "canvas" && d.text) {
    box.append(renderMarkdown(d.text));
  } else if (card.tool === "tasks" && Array.isArray(d.tasks) && d.tasks.length) {
    const ul = el("ul", "tasks");
    for (const t of d.tasks) {
      const li = el("li", "task " + (t.state || ""));
      li.append(el("span", "task-key", t.key || ""), el("span", "task-state", t.state || ""), el("span", "", t.title || ""));
      ul.append(li);
    }
    box.append(ul);
  } else if (card.output) {
    box.append(el("pre", "", card.output));
  }
  if (before && before.parentNode === thread) thread.insertBefore(box, before);
  else thread.append(box);
  thread.parentElement.scrollTop = thread.parentElement.scrollHeight;
  return box;
}

// Ask before a command runs in the sandbox. Resolves true for Run.
// "Allow for this chat" skips the question until the chat is reset.
let allowAll = false;
export function resetAllow() { allowAll = false; }

export function askConfirm(thread, command) {
  if (allowAll) return Promise.resolve(true);
  return new Promise((resolve) => {
    const box = el("div", "tool ask");
    const top = el("div", "tool-top");
    top.append(el("b", "", "Run in EncryptidVM?"));
    box.append(top, el("pre", "", command));
    const acts = el("div", "acts");
    const run = el("button", "go", "Run");
    const always = el("button", "", "Allow for this chat");
    const no = el("button", "", "Don't run");
    for (const b of [run, always, no]) b.type = "button";
    acts.append(run, always, no);
    box.append(acts);
    thread.append(box);
    thread.parentElement.scrollTop = thread.parentElement.scrollHeight;
    const done = (ok, remember) => {
      if (remember) allowAll = true;
      acts.remove();
      box.classList.add(ok ? "ran" : "skipped");
      top.firstChild.textContent = ok ? "Running in EncryptidVM" : "Not run";
      resolve(ok);
    };
    run.onclick = () => done(true, false);
    always.onclick = () => done(true, true);
    no.onclick = () => done(false, false);
    run.focus();
  });
}
