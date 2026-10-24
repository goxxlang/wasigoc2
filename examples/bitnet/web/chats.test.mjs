import assert from "node:assert/strict";
import { createMind } from "./mind.js";
import { createChats, titleOf, transcript, segmentPrompt, rollPrompt } from "./chats.js";

// Every commit goes to the log; a fresh mind replayed from it sees the same chats.
const log = [];
let clock = 1000;
const mind = createMind({ onCommit: (r) => log.push(r) });
const chats = createChats(mind, { now: () => clock++ });

const a = chats.create("How long is Lake Champlain, and what fish live in it?");
assert.equal(chats.meta(a).title, "How long is Lake Champlain, and what fish live…");
assert.equal(titleOf("/run ls -la"), "ls -la");
const u0 = chats.add(a, { role: "user", text: "How long is Lake Champlain?" });
chats.add(a, { role: "assistant", text: "About 107 miles." });
chats.add(a, { role: "card", text: "x", card: { tool: "notes", output: "x" } });
const u1 = chats.add(a, { role: "user", text: "What fish live there?" });
chats.add(a, { role: "assistant", text: "Lake trout, salmon and walleye." });
assert.equal(u0, 0);
assert.equal(u1, 3);
assert.equal(chats.messages(a).length, 5);
assert.equal(chats.messages(a)[2].card.tool, "notes");

const b = chats.create("Draft a short note");
clock += 10;
chats.add(b, { role: "user", text: "Draft a short note" });
assert.deepEqual(chats.list().map((c) => c.id), [b, a]);

// Long messages keep their whole text (the store's default cap is 800).
const long = "x".repeat(5000);
chats.add(b, { role: "assistant", text: long });
assert.equal(chats.messages(b)[1].text.length, 5000);

// Context before any summary: the recent turns, without the message being sent.
let ctx = chats.gather(a, "fish", u1);
assert.match(ctx, /Recent messages:\nYou: How long is Lake Champlain\?\nChamp: About 107 miles\./);
assert.doesNotMatch(ctx, /What fish/);

// A segment covers 0..4, and the rolling summary has versions.
chats.addSegment(a, 0, 4, "They asked the lake's length (107 miles) and its fish (trout, salmon, walleye).");
chats.setRoll(a, "Lake Champlain: 107 miles; trout, salmon, walleye.");
chats.setRoll(a, "Lake Champlain is 107 miles long and has trout, salmon and walleye.");
assert.equal(chats.covered(a), 4);
assert.equal(chats.rollHistory(a).length, 2);
const u2 = chats.add(a, { role: "user", text: "Is it deep?" });
ctx = chats.gather(a, "how deep", u2);
assert.match(ctx, /^Summary of this chat so far: Lake Champlain is 107 miles long/);
assert.match(ctx, /Earlier parts of this chat:\n- They asked/);
assert.doesNotMatch(ctx, /Recent messages/);
chats.add(a, { role: "assistant", text: "Up to 400 feet." });
const u3 = chats.add(a, { role: "user", text: "Thanks" });
ctx = chats.gather(a, "thanks", u3);
assert.match(ctx, /Recent messages:\nYou: Is it deep\?\nChamp: Up to 400 feet\./);

// The budget keeps the newest turns.
const small = chats.gather(a, "x", u3, 120);
assert.ok(small.length <= 240, small);

// Edit: everything from seq 3 goes, and so does a segment that covered it.
chats.truncate(a, 3);
assert.deepEqual(chats.messages(a).map((m) => m.seq), [0, 1, 2]);
assert.equal(chats.covered(a), -1);

// Rename, delete.
chats.rename(a, "Lake facts");
assert.equal(chats.meta(a).title, "Lake facts");
chats.remove(b);
assert.deepEqual(chats.list().map((c) => c.id), [a]);

// Replay into a new mind gives the same state.
const again = createMind();
for (const r of log) again.replay(r.txn, r.page, r.key, r.value);
const chats2 = createChats(again);
assert.deepEqual(chats2.list().map((c) => [c.id, c.title]), [[a, "Lake facts"]]);
assert.deepEqual(chats2.messages(a).map((m) => m.text), chats.messages(a).map((m) => m.text));
assert.equal(chats2.roll(a), chats.roll(a));
// Commits after a replay continue the txn numbers.
const next = again.commit("chat", a + "/meta", "{}");
assert.equal(next, log[log.length - 1].txn + 1);

assert.equal(transcript([{ role: "user", text: "a" }, { role: "card", text: "zz" }, { role: "assistant", text: "b" }]), "You: a\nChamp: b");
assert.match(segmentPrompt("You: a"), /You: a$/);
assert.match(rollPrompt("one", "two"), /Earlier: one\n\nLater: two$/);

console.log("chats tests ok");
