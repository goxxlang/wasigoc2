// Private hop for live search. Chrome stays on this host.
// The page still calls /api/ext/search. That host calls this one.
import http from "node:http";
import { askDuck } from "./duckai.mjs";

const token = process.env.CHAMP_DUCK_TOKEN || "";
const port = Number(process.env.CHAMP_DUCK_PORT || "9225");
const host = process.env.CHAMP_DUCK_BIND || "127.0.0.1";

const server = http.createServer(async (req, res) => {
  const u = new URL(req.url || "/", "http://127.0.0.1");
  if (req.method !== "GET" || u.pathname !== "/ask") {
    res.writeHead(404);
    res.end();
    return;
  }
  if (token && req.headers["x-champ-duck"] !== token) {
    res.writeHead(403);
    res.end();
    return;
  }
  const q = (u.searchParams.get("q") || "").replace(/\s+/g, " ").trim().slice(0, 200);
  if (!q) {
    res.writeHead(400, { "content-type": "application/json; charset=utf-8" });
    res.end(JSON.stringify({ ok: false, output: "" }));
    return;
  }
  let output = "";
  try {
    output = await askDuck(q);
  } catch {
    output = "";
  }
  res.writeHead(200, { "content-type": "application/json; charset=utf-8" });
  res.end(JSON.stringify({ ok: !!output, output: output || "" }));
});

server.listen(port, host);
