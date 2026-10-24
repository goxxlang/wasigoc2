import assert from "node:assert/strict";
import { createStore } from "./mvcc.js";
import { buildLut, embedText, cosine, packEmbedModule, unpackEmbedModule, lutBytes, codeAt } from "./embed.js";
import { createMind, thoughtPrompt, explainPrompt } from "./mind.js";

const store = createStore();
const a = store.commit("think", "q/1", "first");
const mid = store.head();
store.commit("think", "q/1", "second");
assert.equal(store.read("think", "q/1", mid), "first");
assert.equal(store.read("think", "q/1"), "second");
store.commit("think", "q/2", "edge");
const snap = store.scan("think", "q/", store.head());
assert.deepEqual(snap.map((r) => r.value), ["second", "edge"]);

const lut = buildLut();
assert.equal(lut.length, 256 * 2);
assert.ok([-1, 0, 1].includes(codeAt(lut, 3, 5)));
const lake = embedText("length of lake champlain", lut);
const again = embedText("lake champlain length", lut);
const note = embedText("draft a short note about lunch", lut);
assert.ok(cosine(lake, again) > cosine(lake, note));
assert.ok(cosine(lake, lake) > 0.99);

const bnz = packEmbedModule(lut);
assert.equal(String.fromCharCode(bnz[0], bnz[1], bnz[2], bnz[3]), "BNZ2");
const back = unpackEmbedModule(bnz);
assert.equal(back.length, lut.length);
assert.equal(Buffer.from(lutBytes(back)).equals(Buffer.from(lutBytes(lut))), true);

const mind = createMind();
assert.equal(mind.modules().map((m) => m.role).join(","), "chat,embed");
const id = "42";
mind.commit("think", id + "/1", "The lake is long.");
mind.commit("think", id + "/2", "The length is about 107 miles.");
const versions = mind.scan("think", id + "/", mind.head()).map((r) => r.value);
assert.match(thoughtPrompt(2, "How long is it?", versions[0]), /Step 2/);
assert.match(explainPrompt("How long is it?", versions, ""), /107 miles/);
const txn = mind.learn("How long is the lake?", "About 107 miles.");
assert.ok(mind.modules().some((m) => m.id === "learn-" + txn && m.codec === "mvcc"));
assert.match(mind.recall("how long is the lake"), /107 miles/);
assert.equal(mind.recall("unrelated pottery glaze recipe"), "");
console.log("mind ok");
console.log("cosine lake/again", cosine(lake, again).toFixed(3), "lake/note", cosine(lake, note).toFixed(3));
