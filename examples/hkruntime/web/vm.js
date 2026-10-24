// Champ's exec tool: EncryptidVM on this machine, over its MCP endpoint.
// One MCP session per page; the first command opens a sandbox on the first
// host EncryptidVM lists. Commands run in that guest shell,
// never on the host, and EncryptidVM records each one so it can be undone.
// The endpoint is http://127.0.0.1:8870/mcp unless this browser's localStorage
// "champ.vm" names another; nothing in the page's URL can choose it.

const PROTOCOL = "2025-03-26";

export function vmEndpoint(storage) {
  let saved = "";
  try { saved = (storage && storage.getItem("champ.vm")) || ""; } catch { saved = ""; }
  return saved || "http://127.0.0.1:8870/mcp";
}

// The MCP answer is JSON, or an SSE stream whose data lines carry it.
function rpcBody(text) {
  const t = String(text || "").trim();
  if (t.startsWith("{")) return JSON.parse(t);
  let last = null;
  for (const line of t.split(/\r?\n/)) {
    if (line.startsWith("data:")) {
      try { last = JSON.parse(line.slice(5).trim()); } catch { /* not this line */ }
    }
  }
  if (!last) throw new Error("unreadable answer");
  return last;
}

export function createVM(opts) {
  const o = opts || {};
  const endpoint = o.endpoint || "http://127.0.0.1:8870/mcp";
  const f = o.fetch || ((...a) => fetch(...a));
  let session = "";
  let open = false;
  let seq = 0;

  async function post(method, params, notify) {
    const headers = { "Content-Type": "application/json", Accept: "application/json, text/event-stream", "MCP-Protocol-Version": PROTOCOL };
    if (session) headers["Mcp-Session-Id"] = session;
    const msg = { jsonrpc: "2.0", method };
    if (params) msg.params = params;
    if (!notify) msg.id = ++seq;
    const r = await f(endpoint, { method: "POST", headers, body: JSON.stringify(msg) });
    const sid = r.headers.get("Mcp-Session-Id");
    if (sid) session = sid;
    if (notify) return null;
    const text = await r.text();
    if (!r.ok) throw new Error((text || r.statusText || "error").trim().slice(0, 200));
    const body = rpcBody(text);
    if (body.error) throw new Error(body.error.message || "error");
    return body.result;
  }

  async function tool(name, args) {
    const res = await post("tools/call", { name, arguments: args || {} });
    const text = ((res && res.content) || []).map((c) => c.text || "").join("\n");
    return { text, isError: !!(res && res.isError) };
  }

  async function ensure() {
    if (!session) {
      await post("initialize", { protocolVersion: PROTOCOL, capabilities: {}, clientInfo: { name: "champ", version: "1" } });
      await post("notifications/initialized", null, true);
      open = false;
    }
    if (open) return;
    let host = o.host || "";
    if (!host) {
      const hosts = await tool("list_hosts");
      if (hosts.isError) throw new Error(hosts.text);
      host = (hosts.text.split("\n")[0] || "").split("\t")[0].trim();
      if (!host || host === "no hosts") throw new Error("EncryptidVM has no host. Run tunneltug vm up.");
    }
    const r = await tool("sandbox_open", { host });
    if (r.isError) throw new Error(r.text);
    open = true;
  }

  return {
    endpoint,
    async exec(command) {
      const cmd = String(command || "").trim();
      if (!cmd) return { output: "Missing command.", exit_code: 1 };
      try {
        await ensure();
        let r = await tool("sandbox_exec", { command: cmd });
        if (r.isError && /not open|session/i.test(r.text)) {
          session = "";
          await ensure();
          r = await tool("sandbox_exec", { command: cmd });
        }
        const m = /\nexit: (-?\d+)/.exec("\n" + r.text);
        const code = m ? Number(m[1]) : (r.isError ? 1 : 0);
        return { output: r.text || "(no output)", exit_code: code };
      } catch (e) {
        session = "";
        open = false;
        const why = String((e && e.message) || e);
        if (/Failed to fetch|NetworkError|ECONNREFUSED|fetch failed|Load failed/i.test(why)) {
          return { output: "EncryptidVM is not answering at " + endpoint + ". Start it with: encryptidvm serve", exit_code: 1 };
        }
        return { output: "EncryptidVM: " + why, exit_code: 1 };
      }
    },
    async close() {
      if (!session) return;
      try { await f(endpoint, { method: "DELETE", headers: { "Mcp-Session-Id": session } }); } catch { /* gone */ }
      session = "";
      open = false;
    },
  };
}
