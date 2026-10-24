// Markdown for Champ's answers, built as DOM nodes (never innerHTML), so text
// from the model or a service cannot inject markup. Covers what chat answers
// use: headings, paragraphs, lists (with task boxes), quotes, tables, rules,
// fenced code with a copy button, and inline code, bold, italic and links.

function el(tag, cls, text) {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (text != null) e.textContent = text;
  return e;
}

// Inline: `code`, **bold**, *italic* / _italic_, [text](http…), bare http(s) links.
const INLINE = /(`+)([\s\S]+?)\1|\*\*([^*\n]+)\*\*|__([^_\n]+)__|\*([^*\n]+)\*|(?<![\w])_([^_\n]+)_(?![\w])|\[([^\]\n]+)\]\((https?:\/\/[^\s)]+)\)|(https?:\/\/[^\s<>()]+[^\s<>().,;:!?'"])/g;

export function inline(parent, text) {
  const s = String(text || "");
  let at = 0;
  for (const m of s.matchAll(INLINE)) {
    if (m.index > at) parent.append(s.slice(at, m.index));
    if (m[2] != null) parent.append(el("code", "", m[2]));
    else if (m[3] != null || m[4] != null) { const b = el("strong"); inline(b, m[3] || m[4]); parent.append(b); }
    else if (m[5] != null || m[6] != null) { const i = el("em"); inline(i, m[5] || m[6]); parent.append(i); }
    else {
      const a = el("a", "", m[7] != null ? m[7] : m[9]);
      a.href = m[8] || m[9];
      a.target = "_blank";
      a.rel = "noopener noreferrer";
      parent.append(a);
    }
    at = m.index + m[0].length;
  }
  if (at < s.length) parent.append(s.slice(at));
}

function codeBlock(lang, code) {
  const wrap = el("div", "code");
  const bar = el("div", "code-bar");
  bar.append(el("span", "", lang || "code"));
  const copy = el("button", "copy", "Copy");
  copy.type = "button";
  copy.onclick = () => copyText(code, copy);
  bar.append(copy);
  const pre = el("pre");
  pre.append(el("code", "", code));
  wrap.append(bar, pre);
  return wrap;
}

export function copyText(text, button) {
  const done = () => {
    if (!button) return;
    const was = button.textContent;
    button.textContent = "Copied";
    setTimeout(() => { button.textContent = was; }, 1200);
  };
  if (navigator.clipboard && navigator.clipboard.writeText) {
    navigator.clipboard.writeText(text).then(done, done);
    return;
  }
  const ta = el("textarea");
  ta.value = text;
  document.body.append(ta);
  ta.select();
  try { document.execCommand("copy"); } catch { /* nothing to copy with */ }
  ta.remove();
  done();
}

function cells(line) {
  return line.trim().replace(/^\|/, "").replace(/\|$/, "").split("|").map((c) => c.trim());
}

export function renderMarkdown(md, cls) {
  const root = el("div", cls || "md");
  const lines = String(md || "").replace(/\r\n/g, "\n").split("\n");
  let para = [];
  const flush = () => {
    if (!para.length) return;
    const p = el("p");
    para.forEach((l, i) => { if (i) p.append(el("br")); inline(p, l); });
    root.append(p);
    para = [];
  };
  for (let i = 0; i < lines.length; i++) {
    const raw = lines[i];
    const t = raw.trim();
    let m;
    if ((m = /^(`{3,}|~{3,})\s*([\w+#.-]*)/.exec(t))) {
      flush();
      const fence = m[1];
      const code = [];
      for (i++; i < lines.length && !lines[i].trim().startsWith(fence); i++) code.push(lines[i]);
      root.append(codeBlock(m[2], code.join("\n")));
      continue;
    }
    if (!t) { flush(); continue; }
    if (/^(-{3,}|\*{3,}|_{3,})$/.test(t)) { flush(); root.append(el("hr")); continue; }
    if ((m = /^(#{1,6})\s+(.*?)\s*#*$/.exec(t))) {
      flush();
      const h = el("h" + Math.min(6, m[1].length + 2));
      inline(h, m[2]);
      root.append(h);
      continue;
    }
    if (t.startsWith("|") && i + 1 < lines.length && /^\|?\s*:?-{2,}/.test(lines[i + 1].trim())) {
      flush();
      const table = el("table");
      const tr = el("tr");
      for (const c of cells(t)) { const th = el("th"); inline(th, c); tr.append(th); }
      const thead = el("thead");
      thead.append(tr);
      table.append(thead);
      const tbody = el("tbody");
      for (i += 2; i < lines.length && lines[i].trim().startsWith("|"); i++) {
        const row = el("tr");
        for (const c of cells(lines[i])) { const td = el("td"); inline(td, c); row.append(td); }
        tbody.append(row);
      }
      i--;
      table.append(tbody);
      const scroll = el("div", "table");
      scroll.append(table);
      root.append(scroll);
      continue;
    }
    if (/^(?:[-*+]|\d+[.)])\s+/.test(t)) {
      flush();
      const ordered = /^\d/.test(t);
      const list = el(ordered ? "ol" : "ul");
      for (; i < lines.length; i++) {
        const lt = lines[i].trim();
        const item = /^(?:[-*+]|\d+[.)])\s+(.*)$/.exec(lt);
        if (!item) {
          if (lt && list.lastChild && /^\s{2,}/.test(lines[i])) { list.lastChild.append(" "); inline(list.lastChild, lt); continue; }
          break;
        }
        const li = el("li");
        const task = /^\[([ xX])\]\s+(.*)$/.exec(item[1]);
        if (task) {
          const box = el("input");
          box.type = "checkbox";
          box.disabled = true;
          box.checked = task[1] !== " ";
          li.className = "task-item";
          li.append(box, " ");
          inline(li, task[2]);
        } else inline(li, item[1]);
        list.append(li);
      }
      i--;
      root.append(list);
      continue;
    }
    if (t.startsWith(">")) {
      flush();
      const q = [];
      for (; i < lines.length && lines[i].trim().startsWith(">"); i++) q.push(lines[i].trim().replace(/^>\s?/, ""));
      i--;
      const bq = el("blockquote");
      bq.append(renderMarkdown(q.join("\n"), "md-inner"));
      root.append(bq);
      continue;
    }
    para.push(t);
  }
  flush();
  return root;
}
