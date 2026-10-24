// Local lookup for Champ. The page calls this through the dev server.
// Public http(s) only. Private addresses, localhost, and link-local are refused.
import dns from "node:dns/promises";
import net from "node:net";
import { askDuck } from "./duckai.mjs";

const MAX_BYTES = 400_000;
const MAX_REDIRECTS = 3;

function clean(s) {
  return String(s || "").replace(/[\u0000-\u0008\u000b\u000c\u000e-\u001f]/g, " ").replace(/\s+/g, " ").trim();
}

export function isPrivateAddress(ip) {
  const kind = net.isIP(ip);
  if (kind === 4) {
    const p = ip.split(".").map(Number);
    if (p.some((n) => Number.isNaN(n))) return true;
    if (p[0] === 0 || p[0] === 10 || p[0] === 127) return true;
    if (p[0] === 169 && p[1] === 254) return true;
    if (p[0] === 172 && p[1] >= 16 && p[1] <= 31) return true;
    if (p[0] === 192 && p[1] === 168) return true;
    if (p[0] === 100 && p[1] >= 64 && p[1] <= 127) return true;
    return false;
  }
  if (kind === 6) {
    const s = ip.toLowerCase();
    if (s === "::" || s === "::1") return true;
    if (s.startsWith("fe80:") || s.startsWith("fc") || s.startsWith("fd")) return true;
    if (s.startsWith("::ffff:")) return isPrivateAddress(s.slice(7));
    return false;
  }
  return true;
}

function blockedHost(host) {
  const h = host.toLowerCase().replace(/\.$/, "").replace(/^\[|\]$/g, "");
  if (!h) return true;
  if (h === "localhost" || h.endsWith(".localhost") || h.endsWith(".local") || h.endsWith(".internal")) return true;
  if (h === "metadata.google.internal") return true;
  return false;
}

export async function allowedTarget(raw) {
  let u;
  try { u = new URL(String(raw)); } catch { return null; }
  if (u.username || u.password) return null;
  if (u.protocol !== "http:" && u.protocol !== "https:") return null;
  if (blockedHost(u.hostname)) return null;
  const host = u.hostname.replace(/^\[|\]$/g, "");
  if (net.isIP(host)) return isPrivateAddress(host) ? null : u;
  let addrs;
  try { addrs = await dns.lookup(host, { all: true, verbatim: true }); }
  catch { return null; }
  if (!addrs.length || addrs.some((a) => isPrivateAddress(a.address))) return null;
  return u;
}

function htmlText(html) {
  return clean(String(html)
    .replace(/<script[\s\S]*?<\/script>/gi, " ")
    .replace(/<style[\s\S]*?<\/style>/gi, " ")
    .replace(/<[^>]+>/g, " ")
    .replace(/&nbsp;/gi, " ")
    .replace(/&amp;/gi, "&")
    .replace(/&lt;/gi, "<")
    .replace(/&gt;/gi, ">")
    .replace(/&quot;/gi, '"')
    .replace(/&#39;|&apos;/gi, "'")
    .replace(/&#(\d+);/g, (_, n) => {
      const c = Number(n);
      return c > 31 && c < 65536 ? String.fromCharCode(c) : " ";
    }));
}

// Wiki templates and the JSON hung on data-mw are markup, not facts.
function dropTemplates(s) {
  let t = String(s);
  for (let i = 0; i < 8; i++) {
    const n = t.replace(/\{\{[^{}]*\}\}/g, " ");
    if (n === t) break;
    t = n;
  }
  return clean(t);
}

// The article's paragraphs. A full page starts with menus, and the first 900
// characters of that are navigation, which the model then treats as facts.
export function pageText(html) {
  const raw = String(html)
    .replace(/<script[\s\S]*?<\/script>/gi, " ")
    .replace(/<style[\s\S]*?<\/style>/gi, " ")
    .replace(/\sdata-mw=(['"])[\s\S]*?\1/gi, "");
  const paras = [];
  const re = /<p\b([^>]*)>([\s\S]*?)<\/p>/gi;
  let m;
  let n = 0;
  while ((m = re.exec(raw))) {
    if (/mw-empty-elt/i.test(m[1])) continue;
    const inner = m[2].replace(/<sup\b[\s\S]*?<\/sup>/gi, " ");
    const t = dropTemplates(htmlText(inner));
    if (t.length < 40) continue;
    paras.push(t);
    n += t.length + 1;
    if (n >= 900) break;
  }
  const body = paras.join(" ");
  if (body.length >= 80) return clipProse(body);
  return clipProse(dropTemplates(htmlText(raw)));
}

function clipProse(s) {
  if (s.length <= 900) return s;
  const cut = s.slice(0, 900);
  const dot = cut.lastIndexOf(". ");
  if (dot >= 200) return cut.slice(0, dot + 1);
  return cut;
}

function ddgUrl(href) {
  const raw = String(href || "").replace(/&amp;/g, "&");
  const m = /[?&]uddg=([^&]+)/.exec(raw);
  if (!m) return "";
  try { return decodeURIComponent(m[1]); } catch { return ""; }
}

export function parseDdg(html) {
  const hits = [];
  const re = /<a\b([^>]*\bclass="result__a"[^>]*)>([\s\S]*?)<\/a>/gi;
  let m;
  while ((m = re.exec(html))) {
    const href = /href="([^"]+)"/i.exec(m[1]);
    const url = ddgUrl(href && href[1]);
    if (!/^https?:\/\//i.test(url)) continue;
    if (/ad_domain=|\/y\.js/i.test(url)) continue;
    const title = htmlText(m[2]).slice(0, 160);
    if (!title) continue;
    hits.push({ title, url, snippet: "" });
    if (hits.length >= 8) break;
  }
  const snips = [];
  const sr = /<a\b([^>]*class="result__snippet"[^>]*)>([\s\S]*?)<\/a>/gi;
  let s;
  while ((s = sr.exec(html))) {
    const href = /href="([^"]+)"/i.exec(s[1]);
    const url = ddgUrl(href && href[1]);
    if (/ad_domain=|\/y\.js/i.test(url)) continue;
    snips.push(htmlText(s[2]).slice(0, 180));
  }
  let organic = 0;
  for (const hit of hits) {
    if (organic < snips.length) hit.snippet = snips[organic] || "";
    organic++;
  }
  const out = [];
  const seen = new Set();
  for (const hit of hits) {
    if (seen.has(hit.url)) continue;
    seen.add(hit.url);
    out.push(hit);
    if (out.length >= 5) break;
  }
  return out;
}

async function readLimited(res) {
  const reader = res.body && res.body.getReader ? res.body.getReader() : null;
  if (!reader) return "";
  const chunks = [];
  let n = 0;
  while (n < MAX_BYTES) {
    const step = await reader.read();
    if (step.done) break;
    chunks.push(Buffer.from(step.value));
    n += step.value.length;
  }
  reader.cancel().catch(() => {});
  return Buffer.concat(chunks).toString("utf8");
}

async function fetchPublic(raw) {
  let current = await allowedTarget(raw);
  if (!current) return null;
  for (let hop = 0; hop <= MAX_REDIRECTS; hop++) {
    const res = await fetch(current.href, {
      redirect: "manual",
      signal: AbortSignal.timeout(8000),
      headers: { "user-agent": "EncryptidChamp/1.0", accept: "text/html,application/json" },
    });
    if (res.status >= 300 && res.status < 400) {
      const loc = res.headers.get("location");
      if (!loc) return null;
      current = await allowedTarget(new URL(loc, current).href);
      if (!current) return null;
      continue;
    }
    if (!res.ok) return null;
    return readLimited(res);
  }
  return null;
}

function formatHits(hits) {
  if (!hits.length) return "Nothing matched.";
  return hits.slice(0, 5).map((h, i) => {
    const bit = h.snippet ? "\n" + h.snippet : "";
    return (i + 1) + ". " + h.title + "\n" + h.url + bit;
  }).join("\n\n").slice(0, 700);
}

async function wikiSearch(q) {
  const url = "https://en.wikipedia.org/w/api.php?action=opensearch&format=json&limit=5&namespace=0&search=" + encodeURIComponent(q);
  const body = await fetchPublic(url);
  if (!body) return [];
  let data;
  try { data = JSON.parse(body); } catch { return []; }
  if (!Array.isArray(data) || !Array.isArray(data[1])) return [];
  const titles = data[1];
  const blurbs = Array.isArray(data[2]) ? data[2] : [];
  const urls = Array.isArray(data[3]) ? data[3] : [];
  const hits = [];
  for (let i = 0; i < titles.length && hits.length < 5; i++) {
    if (!urls[i]) continue;
    hits.push({ title: clean(titles[i]).slice(0, 160), url: urls[i], snippet: clean(blurbs[i] || "").slice(0, 180) });
  }
  return hits;
}

export async function searchWeb(q) {
  const query = clean(q).slice(0, 200);
  if (!query) return "Missing search query.";
  if (process.env.CHAMP_DUCK !== "0") {
    try {
      const answer = await askDuck(query);
      if (answer) return answer;
    } catch { /* page search below */ }
  }
  let hits = [];
  try {
    const html = await fetchPublic("https://html.duckduckgo.com/html/?q=" + encodeURIComponent(query));
    if (html) hits = parseDdg(html);
  } catch { hits = []; }
  if (!hits.length) {
    try { hits = await wikiSearch(query); } catch { hits = []; }
  }
  return formatHits(hits);
}

export async function gatherPage(raw) {
  const target = await allowedTarget(raw);
  if (!target) return { ok: false, output: "That address is not available." };
  let html;
  try { html = await fetchPublic(target.href); }
  catch { return { ok: false, output: "The lookup failed." }; }
  if (!html) return { ok: false, output: "The lookup failed." };
  const text = pageText(html);
  if (!text) return { ok: false, output: "That page had no text." };
  return { ok: true, output: text };
}
