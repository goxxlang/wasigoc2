import assert from "node:assert/strict";
import { PICKS, pickedHint, runTool, settleReply, slashCall, toolCallOf, isAgentTool } from "./ext.js";
import { createVM, vmEndpoint } from "./vm.js";
import { createServices, memoryBackend } from "./localsvc.js";

// Direct commands.
assert.deepEqual(slashCall("/run ls -la"), { type: "tool_call", tool: "exec", args: { cmd: "ls -la" } });
assert.deepEqual(slashCall("/note plans/today.md"), { type: "tool_call", tool: "notes", args: { op: "read", path: "plans/today.md" } });
assert.deepEqual(slashCall("/note plans/today.md\n# Today\nship"), { type: "tool_call", tool: "notes", args: { op: "write", path: "plans/today.md", text: "# Today\nship" } });
assert.deepEqual(slashCall("/task Wire 0trust"), { type: "tool_call", tool: "tasks", args: { op: "add", title: "Wire 0trust", body: "" } });
assert.deepEqual(slashCall("/done CHAMP-2"), { type: "tool_call", tool: "tasks", args: { op: "done", id: "CHAMP-2" } });
assert.deepEqual(slashCall("/canvas Plan\n# A"), { type: "tool_call", tool: "canvas", args: { op: "write", title: "Plan", text: "# A" } });
assert.equal(slashCall("/run"), null);
assert.equal(slashCall("run ls"), null);
assert.equal(slashCall("/unknown x"), null);

// The model's JSON form keeps its args for the new tools.
const c = toolCallOf('{"type":"tool_call","tool":"notes","args":{"op":"read","path":"a.md"}}');
assert.deepEqual(c.args, { op: "read", path: "a.md" });
assert.equal(isAgentTool("exec"), true);
assert.equal(isAgentTool("search"), false);

// notes, tasks and canvas run in the page, op inferred when missing. No fetch, no sign-in.
const services = createServices({ backend: memoryBackend() });
const nofetch = async () => { throw new Error("notes, tasks and canvas must not fetch"); };
let r = await runTool({ tool: "notes", args: { path: "a.md", text: "x" } }, nofetch, { services });
assert.equal(r.exit_code, 0);
assert.equal(r.output, "Saved a.md.");
r = await runTool({ tool: "notes", args: { path: "a.md" } }, nofetch, { services });
assert.equal(r.output, "x");
const site = (call) => runTool(call, nofetch, { services });

// A write is shown and ends the turn; a read feeds Champ the result and shows the card.
let step = await settleReply('{"type":"tool_call","tool":"notes","args":{"op":"write","path":"a.md","text":"x"}}', 0, site);
assert.equal(step.action, "show");
assert.equal(step.text, "Saved a.md.");
step = await settleReply('{"type":"tool_call","tool":"notes","args":{"op":"read","path":"a.md"}}', 0, site);
assert.equal(step.action, "follow");
assert.match(step.wire, /^Results:\nx\n/);
assert.equal(step.card.tool, "notes");
// Agent tools may chain past the two rounds search gets, up to four.
step = await settleReply('{"type":"tool_call","tool":"notes","args":{"op":"read","path":"a.md"}}', 3, site);
assert.equal(step.action, "follow");
step = await settleReply('{"type":"tool_call","tool":"notes","args":{"op":"read","path":"a.md"}}', 4, site);
assert.equal(step.action, "finish");
step = await settleReply('{"type":"tool_call","tool":"search","args":{"q":"x"}}', 2, async () => ({ output: "", exit_code: 0 }));
assert.equal(step.action, "finish");

// exec: a fake EncryptidVM speaking MCP streamable HTTP.
function fakeVM() {
  const seen = [];
  let open = false;
  const fetchImpl = async (url, init) => {
    const msg = JSON.parse(init.body || "{}");
    seen.push(msg.method + (msg.params && msg.params.name ? ":" + msg.params.name : ""));
    const headers = new Map([["mcp-session-id", "s1"]]);
    const reply = (result, isSSE) => ({
      ok: true,
      headers: { get: (k) => headers.get(k.toLowerCase()) || null },
      text: async () => isSSE ? "event: message\ndata: " + JSON.stringify({ jsonrpc: "2.0", id: msg.id, result }) + "\n\n" : JSON.stringify({ jsonrpc: "2.0", id: msg.id, result }),
    });
    if (msg.method === "initialize") return reply({ protocolVersion: "2025-03-26" });
    if (msg.method === "notifications/initialized") return { ok: true, headers: { get: () => null }, text: async () => "" };
    assert.equal(init.headers["Mcp-Session-Id"], "s1");
    const name = msg.params.name;
    const text = (t, isError) => reply({ content: [{ type: "text", text: t }], isError: !!isError }, name === "sandbox_exec");
    if (name === "list_hosts") return text("eda\tCA01");
    if (name === "sandbox_open") { assert.equal(msg.params.arguments.host, "eda"); open = true; return text("sandbox open\nid: sb1"); }
    if (name === "sandbox_exec") {
      assert.equal(open, true);
      return text("hello\nexit: " + (msg.params.arguments.command === "false" ? 1 : 0) + "\ncwd: /work", msg.params.arguments.command === "false");
    }
    throw new Error("unexpected " + name);
  };
  return { seen, fetchImpl };
}

const vmf = fakeVM();
const vm = createVM({ fetch: vmf.fetchImpl, endpoint: "http://127.0.0.1:8870/mcp" });
let out = await vm.exec("echo hello");
assert.equal(out.exit_code, 0);
assert.match(out.output, /^hello\nexit: 0/);
out = await vm.exec("false");
assert.equal(out.exit_code, 1);
assert.deepEqual(vmf.seen, ["initialize", "notifications/initialized", "tools/call:list_hosts", "tools/call:sandbox_open", "tools/call:sandbox_exec", "tools/call:sandbox_exec"]);

// exec asks first, and a no stops it.
let asked = "";
r = await runTool({ tool: "exec", args: { cmd: "rm -rf /tmp/x" } }, null, { vm, confirm: async (cmd) => { asked = cmd; return false; } });
assert.equal(asked, "rm -rf /tmp/x");
assert.equal(r.declined, true);
step = await settleReply('{"type":"tool_call","tool":"exec","args":{"cmd":"ls"}}', 0, (call) => runTool(call, null, { vm, confirm: async () => false }));
assert.equal(step.action, "show");
assert.match(step.text, /declined/);
step = await settleReply('{"type":"tool_call","tool":"exec","args":{"cmd":"echo hi"}}', 0, (call) => runTool(call, null, { vm, confirm: async () => true }));
assert.equal(step.action, "follow");
assert.equal(step.card.tool, "exec");

// A VM that does not answer gives a plain message, not a stack trace.
const down = createVM({ fetch: async () => { throw new TypeError("Failed to fetch"); } });
out = await down.exec("ls");
assert.equal(out.exit_code, 1);
assert.match(out.output, /not answering .*encryptidvm serve/);

assert.equal(vmEndpoint({ getItem: () => "http://127.0.0.1:9999/mcp" }), "http://127.0.0.1:9999/mcp");
assert.equal(vmEndpoint({ getItem: () => null }), "http://127.0.0.1:8870/mcp");
assert.equal(vmEndpoint(null), "http://127.0.0.1:8870/mcp");

// The + menu: each pick names a tool the page runs, and its form parses as that tool.
for (const p of PICKS) assert.equal(toolCallOf(p.form.replace(/ \(op is[^)]*\)$/, "")).tool, p.tool);
assert.equal(pickedHint([]), "");
assert.equal(pickedHint(["nope"]), "");
assert.match(pickedHint(["notes"]), /^The person chose the notes tool for this\. Reply with only \{"type":"tool_call","tool":"notes"/);
assert.match(pickedHint(["tasks", "canvas"]), /tools for this: tasks, canvas\..*\n\{"type":"tool_call","tool":"tasks"[\s\S]*\n\{"type":"tool_call","tool":"canvas"/);

console.log("agent tests ok");
