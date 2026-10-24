import assert from "node:assert/strict";
import { cleanNotePath, createServices, findTicket, markdownBlocks, memoryBackend, slugOf, taskState } from "./localsvc.js";

const svc = createServices({ backend: memoryBackend() });
const run = (tool, args) => svc.run(tool, args);

// notes: GitMesh paths and commits.
assert.equal(cleanNotePath("/plans//today.md"), "plans/today.md");
assert.equal(cleanNotePath("../x"), "");
assert.equal(cleanNotePath("a/.git/x"), "");
assert.equal(cleanNotePath("a/"), "");
assert.equal(cleanNotePath("a?b"), "");
assert.deepEqual(await run("notes", { op: "list" }), { ok: true, output: "No notes yet.", data: { files: [] } });
let r = await run("notes", { op: "write", path: "plans/today.md", text: "# Today\nship" });
assert.equal(r.ok, true);
assert.equal(r.output, "Saved plans/today.md.");
assert.match(r.data.commit, /^[0-9a-f]{40}$/);
await run("notes", { op: "write", path: "ideas.md", text: "i" });
assert.equal((await run("notes", { op: "read", path: "plans/today.md" })).output, "# Today\nship");
assert.equal((await run("notes", { op: "list" })).output, "ideas.md\nplans/today.md");
assert.equal((await run("notes", { op: "list", path: "plans" })).output, "plans/today.md");
assert.equal((await run("notes", { op: "read", path: "nope.md" })).output, "There is no note nope.md.");
assert.equal((await run("notes", { op: "write", path: "" })).ok, false);
assert.equal((await run("notes", { op: "write", path: "big.md", text: "x".repeat((256 << 10) + 1) })).ok, false);
assert.equal((await run("notes", { op: "rm", path: "ideas.md" })).output, "Removed ideas.md.");
assert.equal((await run("notes", { op: "list" })).output, "plans/today.md");
assert.equal((await run("notes", { op: "frob" })).output, "Notes can list, read, write or rm.");
const log = await svc.log();
assert.equal(log.length, 3);
assert.equal(log[0].message, "champ: remove ideas.md");
assert.equal(log[1].parent, log[2].id);

// tasks: TodoTraq project CHAMP, Task type states.
assert.equal(taskState("todo"), "open");
assert.equal(taskState("doing"), "in_progress");
assert.equal(taskState("closed"), "done");
assert.equal(taskState("blocked"), "blocked");
assert.equal(taskState("nope"), "");
assert.equal((await run("tasks", { op: "list" })).output, "No tasks.");
r = await run("tasks", { op: "add", title: "Wire the tools", body: "local" });
assert.equal(r.output, "Added CHAMP-1 [open] Wire the tools.");
assert.equal(r.data.tasks[0].fields.summary, "local");
await run("tasks", { op: "add", title: "Ship" });
assert.equal((await run("tasks", { op: "start", id: "CHAMP-1" })).output, "Updated CHAMP-1 [in_progress] Wire the tools.");
assert.equal((await run("tasks", { op: "done", id: "2" })).output, "Updated CHAMP-2 [done] Ship.");
assert.equal((await run("tasks", { op: "done", id: "#2" })).output, "Updated CHAMP-2 [done] Ship.");
assert.equal((await run("tasks", { op: "update", id: "CHAMP-2", title: "Ship it" })).output, "Updated CHAMP-2 [done] Ship it.");
assert.equal((await run("tasks", { op: "update", id: "CHAMP-2" })).output, "Nothing to change on CHAMP-2.");
assert.equal((await run("tasks", { op: "done", id: "CHAMP-9" })).output, "There is no task CHAMP-9.");
assert.equal((await run("tasks", { op: "list", state: "doing" })).output, "CHAMP-1 [in_progress] Wire the tools.".slice(0, -1));
assert.equal((await run("tasks", { op: "list", state: "zzz" })).output, "State is one of backlog, open, in_progress, blocked, done.");
assert.equal((await run("tasks", { op: "list" })).data.tasks.length, 2);
assert.equal((await run("tasks", { op: "comment", id: "CHAMP-1", text: "halfway" })).output, "Commented on CHAMP-1.");
assert.equal((await run("tasks", { op: "comment", id: "CHAMP-1" })).output, "A comment needs text.");
assert.deepEqual((await svc.comments("CHAMP-1")).map((c) => c.content), ["halfway"]);
assert.equal((await run("tasks", { op: "add" })).output, "A task needs a title.");
assert.equal(findTicket([{ id: "CHAMP-3", key: "CHAMP-3", number: 3 }], "x3"), null);

// canvas: MotionKB blocks from markdown, source kept beside the notes.
assert.deepEqual(markdownBlocks("# Plan\n\n- [x] one\n- two\n> q\npara\nline\n```\ncode\n```"), [
  { type: "heading1", content: "Plan" },
  { type: "todo", content: "one", checked: true },
  { type: "bullet", content: "two" },
  { type: "quote", content: "q" },
  { type: "paragraph", content: "para line" },
  { type: "code", content: "code" },
]);
assert.equal(slugOf("My Plan!"), "my-plan");
assert.equal(slugOf("!!"), "canvas");
assert.equal((await run("canvas", { op: "list" })).output, "No canvases yet.");
r = await run("canvas", { op: "write", title: "My Plan", text: "# Plan\n- a\n- b" });
assert.equal(r.output, 'Canvas "My Plan" has 3 blocks.');
assert.equal(r.data.text, "# Plan\n- a\n- b");
assert.equal((await run("canvas", { op: "read", title: "my plan" })).output, "# Plan\n- a\n- b");
assert.equal((await run("canvas", { op: "list" })).output, "My Plan");
assert.equal((await run("canvas", { op: "write", title: "Empty", text: "  " })).output, "The canvas needs some markdown to show.");
assert.equal((await run("canvas", { op: "read", title: "Nope" })).output, 'There is no canvas "Nope".');
// The canvas state is not a note; its source is.
assert.equal((await run("notes", { op: "list" })).output, "canvas/my-plan.md\nplans/today.md");

// Concurrent adds never share a number.
const many = await Promise.all([1, 2, 3].map((i) => run("tasks", { op: "add", title: "t" + i })));
assert.deepEqual(many.map((x) => x.data.task.key).sort(), ["CHAMP-3", "CHAMP-4", "CHAMP-5"]);

console.log("localsvc tests ok");
