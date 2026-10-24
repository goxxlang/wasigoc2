import assert from "node:assert/strict";
import { ENGINES, KIND, PRESENCE_CAP, ROUTE, JOIN, accept, admit, decide, features, gate, join, mixOf, presenceOf, route, toolsFor } from "./router.js";

// The die's tables, unchanged.
assert.deepEqual(ROUTE, ["stage", "load", "reset", "pass", "stage", "pass", "reset", "pass"]);
assert.equal(route(false, KIND.chat), "stage");
assert.equal(route(false, KIND.tool), "load");
assert.equal(route(true, KIND.tool), "pass");
assert.equal(route(true, KIND.reload), "reset");
assert.equal(route(false, KIND.fence), "pass");
assert.equal(JOIN.filter((x) => x === "emit").length, 1);
assert.equal(join(true, true, true), "emit");
assert.equal(join(true, false, true), "hold");
assert.equal(join(true, true, false), "hold");
assert.equal(accept(KIND.tool, { busy: true, queue: 9 }), "take");
assert.equal(accept(KIND.chat, { busy: false, loading: false, queue: 0 }), "take");
assert.equal(accept(KIND.chat, { busy: true, queue: 0 }), "stall");
assert.equal(accept(KIND.chat, { loading: true, queue: 0 }), "stall");
assert.equal(accept(KIND.reload, { busy: true, queue: 1 }), "take");
assert.equal(accept(KIND.fence, { queue: 2 }), "stall");

// The gate is a distribution over the four engines.
for (const t of ["hi", "Why does ice float? Prove it step by step.", "Write a detailed report on the protocol design."]) {
  const g = gate(features(t));
  assert.ok(Math.abs(g.reduce((a, b) => a + b, 0) - 1) < 1e-9);
}
assert.equal(ENGINES.map((e) => e.tier).join(), "lite,balanced,thinking,pro");

const all = [
  { id: "champ", name: "Hodge 2B4T", tier: "lite" },
  { id: "vaultgemma", name: "VaultGemma 1B", tier: "balanced" },
  { id: "llama3", name: "Llama 3", tier: "thinking" },
  { id: "gpt-oss", name: "gpt-oss", tier: "pro" },
];
let d0;
const at = (text, loaded, o) => decide(Object.assign({ text, mode: "auto", models: all, loaded }, o || {}));

assert.equal(at("hi", "champ").engine.tier, "lite");
assert.equal(at("hi", "champ").swap, false);
assert.equal(at("Why does the derivative of x^2 equal 2x? Prove it step by step.", "").engine.tier, "thinking");
assert.equal(at("Write a detailed, comprehensive report on the architecture and protocol design of a mesh network, covering every part.", "").engine.tier, "balanced");
// The same message with Lite loaded: Balanced wins by less than the capacity, so it stays.
d0 = at("Write a detailed, comprehensive report on the architecture and protocol design of a mesh network, covering every part.", "champ");
assert.equal(d0.engine.tier, "lite");
assert.match(d0.reason, /kept loaded \(Balanced/);
assert.equal(at("Compare the trade-offs and analyze, step by step, a comprehensive architecture and protocol design for a detailed system report.", "").engine.tier, "pro");
// Think on moves the message to the thinking row.
assert.equal(at("hello there friend", "champ", { think: true }).engine.row, 1);

// Engines the server lacks drop out: Balanced goes where the gate puts the rest.
const two = [all[0], all[2]];
let d = decide({ text: "Write a detailed, comprehensive report on the architecture and protocol design of a mesh network.", mode: "auto", models: two, loaded: "" });
assert.equal(d.engine.tier, "lite");
assert.match(d.reason, /Balanced is not on this server/);
d = decide({ text: "Compare the trade-offs and analyze, step by step, a comprehensive architecture and protocol design for a detailed system report.", mode: "auto", models: two, loaded: "champ" });
assert.equal(d.engine.tier, "thinking");
assert.equal(d.swap, true);

// Capacity: a close call stays on the loaded engine.
d = at("Plan a short trip to the lake.", "llama3");
const fresh = at("Plan a short trip to the lake.", "nothing-loaded");
if (fresh.engine.tier !== "thinking") {
  const p = fresh.gates;
  const li = ENGINES.findIndex((e) => e.tier === "thinking");
  const ti = ENGINES.findIndex((e) => e.tier === fresh.engine.tier);
  assert.equal(d.engine.tier, p[ti] - p[li] < 0.2 ? "thinking" : fresh.engine.tier);
}

// Manual mode is the chosen model, whatever the message.
d = decide({ text: "Prove it step by step", mode: "champ", models: all, loaded: "llama3" });
assert.equal(d.model.id, "champ");
assert.equal(d.swap, true);

// Tools: picks first, then strong cues.
assert.deepEqual(toolsFor("save this as a note", []), ["notes"]);
assert.deepEqual(toolsFor("add a task to call the dentist", ["search"]), ["search", "tasks"]);
assert.deepEqual(toolsFor("read https://example.com and summarize it", []), ["gather"]);
assert.deepEqual(toolsFor("what is the capital of France", []), []);
assert.deepEqual(toolsFor("Write me a model card for this model", []), []);

// The four gates stay a mixture. A greeting is almost all lite.
const hi = mixOf("hi");
assert.equal(hi.length, 4);
assert.ok(Math.abs(hi.reduce((a, b) => a + b, 0) - 1) < 1e-6);
assert.ok(hi[0] > 0.9);
const wide = mixOf("Write a detailed, comprehensive report on the architecture and protocol design of a mesh network.");
assert.ok(wide[1] > hi[1]);

// Admission: same direction only.
assert.equal(admit(0.3), true);
assert.equal(admit(0), false);
assert.equal(admit(-0.2), false);
assert.equal(admit(NaN), false);

// Idle gate mass is a subtle presence penalty. A greeting barely leaves lite.
// A message that would have taken every other engine sits on the cap.
assert.ok(presenceOf("hi") > 0 && presenceOf("hi") < 0.05);
assert.equal(presenceOf("Compare the trade-offs and analyze, step by step, a comprehensive architecture and protocol design for a detailed system report."), PRESENCE_CAP);
assert.ok(presenceOf("The capital of France is") < presenceOf("Why does ice float? Prove it step by step."));

console.log("router tests ok");
