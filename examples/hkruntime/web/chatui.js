// The chat list in the side bar and the actions under each message.

import { copyText } from "./markdown.js";

function el(tag, cls, text) {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (text != null) e.textContent = text;
  return e;
}

function groupOf(t, now) {
  const d0 = new Date(now);
  d0.setHours(0, 0, 0, 0);
  const day = 86400000;
  if (t >= d0.getTime()) return "Today";
  if (t >= d0.getTime() - day) return "Yesterday";
  if (t >= d0.getTime() - 7 * day) return "Previous 7 days";
  if (t >= d0.getTime() - 30 * day) return "Previous 30 days";
  return "Older";
}

// items: [{id, title, updated}], newest first. h: {open(id), rename(id, title), remove(id)}
export function renderChatList(box, items, current, h, filter) {
  box.textContent = "";
  const f = String(filter || "").trim().toLowerCase();
  const shown = f ? items.filter((c) => (c.title || "").toLowerCase().includes(f)) : items;
  if (!shown.length) {
    box.append(el("p", "chats-empty", f ? "No chats match." : "Your chats appear here."));
    return;
  }
  const now = Date.now();
  let group = "";
  for (const c of shown) {
    const g = groupOf(c.updated || c.created || now, now);
    if (g !== group) {
      group = g;
      box.append(el("div", "chats-group", g));
    }
    const row = el("div", "chat-row" + (c.id === current ? " on" : ""));
    const open = el("button", "chat-open", c.title || "New chat");
    open.type = "button";
    open.title = c.title || "";
    open.onclick = () => h.open(c.id);
    const ren = el("button", "chat-act", "✎");
    ren.type = "button";
    ren.title = "Rename";
    ren.setAttribute("aria-label", "Rename chat");
    ren.onclick = (e) => {
      e.stopPropagation();
      const input = el("input", "chat-edit");
      input.value = c.title || "";
      row.replaceChildren(input);
      input.focus();
      input.select();
      let done = false;
      const finish = (save) => {
        if (done) return;
        done = true;
        if (save && input.value.trim()) h.rename(c.id, input.value.trim());
        else h.refresh();
      };
      input.onkeydown = (ev) => {
        if (ev.key === "Enter") finish(true);
        if (ev.key === "Escape") finish(false);
      };
      input.onblur = () => finish(true);
    };
    const del = el("button", "chat-act", "🗑");
    del.type = "button";
    del.title = "Delete";
    del.setAttribute("aria-label", "Delete chat");
    del.onclick = (e) => {
      e.stopPropagation();
      if (del.dataset.armed) {
        h.remove(c.id);
        return;
      }
      del.dataset.armed = "1";
      del.textContent = "Delete?";
      del.classList.add("armed");
      setTimeout(() => {
        if (!del.isConnected) return;
        delete del.dataset.armed;
        del.textContent = "🗑";
        del.classList.remove("armed");
      }, 2500);
    };
    row.append(open, ren, del);
    box.append(row);
  }
}

// The row of actions under a message. a: {text, edit?(), regen?()}
export function messageActions(row, a) {
  row.querySelector(".acts-msg")?.remove();
  const bar = el("div", "acts-msg");
  const copy = el("button", "", "Copy");
  copy.type = "button";
  copy.onclick = () => copyText(a.text || "", copy);
  bar.append(copy);
  if (a.edit) {
    const b = el("button", "", "Edit");
    b.type = "button";
    b.onclick = a.edit;
    bar.append(b);
  }
  if (a.regen) {
    const b = el("button", "", "Regenerate");
    b.type = "button";
    b.onclick = a.regen;
    bar.append(b);
  }
  row.append(bar);
  return bar;
}
