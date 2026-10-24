import assert from "node:assert/strict";
import { toolCallOf, toolStream, noteUseful, noteWords, draftText, chatNote, runTool, settleReply, wantsLookup, lookupQuery, wantsSavedNotes, wantsTeach, teachQuery, unknownReply, acceptsLearn, declinesLearn, callAllowed, playedTool } from "./ext.js";
import { duckAnswer } from "./duckai.mjs";
import { isPrivateAddress, allowedTarget, parseDdg, searchWeb, gatherPage } from "./search.mjs";

process.env.CHAMP_DUCK = "0";

assert.equal(wantsLookup("lookup todays weather in Mount Airy, Md"), true);
assert.equal(wantsLookup("search for Lake Champlain"), true);
assert.equal(wantsLookup("what time is it"), false);
assert.equal(wantsLookup("What's the date today?"), false);
assert.equal(wantsLookup("Draft a short note."), false);
assert.equal(wantsSavedNotes("Draft a short note."), false);
assert.equal(wantsSavedNotes("save this as a note"), true);
assert.equal(wantsSavedNotes("show my notes"), true);
assert.equal(wantsSavedNotes("What can you help me with?"), false);
assert.equal(lookupQuery("lookup todays weather in Mount Airy, Md"), "todays weather in Mount Airy, Md");
assert.equal(wantsTeach("Teach yourself how tides work"), true);
assert.equal(wantsTeach("Please learn the population of Frederick County"), true);
assert.equal(wantsTeach("Ask a foundational model to teach you what a culvert is"), true);
assert.equal(wantsTeach("what time is it"), false);
assert.equal(wantsTeach("Draft a short note."), false);
assert.equal(wantsTeach("lookup todays weather in Mount Airy, Md"), false);
assert.equal(teachQuery("Teach yourself how tides work"), "how tides work");
assert.equal(teachQuery("Please learn the population of Frederick County"), "the population of Frederick County");
assert.equal(teachQuery("Ask a foundational model to teach you what a culvert is"), "what a culvert is");
assert.equal(unknownReply('I\'m sorry, I don\'t understand. Could you please clarify what you mean by "Don\'t sweat the technique"?'), true);
assert.equal(unknownReply("Lake Champlain is long and cold. I can draft the note."), false);
assert.equal(unknownReply("Hello."), false);
assert.equal(acceptsLearn("yes"), true);
assert.equal(acceptsLearn("Yes please"), true);
assert.equal(acceptsLearn("Don't sweat the technique"), false);
assert.equal(declinesLearn("no"), true);
assert.equal(declinesLearn("no thanks"), true);
const duckSample = "You said Weather in Mount Airy Maryland today. Duck.ai said GPT-6 Luna It’s sunny in Mount Airy today, with a high near 77°F. National Weather Service Read More Mount Airy MD weather.gov Duck.ai works best in our app";
const duckText = duckAnswer(duckSample);
assert.match(duckText, /77°F/);
assert.match(duckText, /Mount Airy/);
assert.equal(/GPT|Luna|Read More|works best/i.test(duckText), false);
const named = duckAnswer("Duck.ai said GPT-6 Luna Mount Airy is forecast to be sunny today, with a high near 77°F. National Weather Service Duck.ai works best");
assert.match(named, /^Mount Airy is forecast/);
assert.match(named, /National Weather Service/);
const cited = duckAnswer("Duck.ai said Mount Airy is forecast to be sunny today, with a high near 77°F. Tonight should be clear, with a low around 63°F.< Citation src=\"5\"> Related Searches Mount Airy MD hourly weather 2nd opinion");
assert.match(cited, /63°F\.$/);
assert.equal(/Citation|Related Searches|2nd opinion/i.test(cited), false);
assert.equal(duckAnswer("You said hi. Duck.ai said Searching the web Mount Airy weather"), "");

const prose = "Lake Champlain is long and cold. I can draft the note.";
assert.equal(toolCallOf(prose), null);
assert.equal(toolCallOf("The time is 7."), null);
assert.equal(toolCallOf("{not json"), null);
assert.equal(toolCallOf('{"type":"tool_call","tool":"shell","args":{"cmd":"id"}}').tool, "shell");
assert.equal((await runTool(toolCallOf('{"type":"tool_call","tool":"shell","args":{"cmd":"id"}}'))).output, "Unknown tool.");

const search = toolCallOf('{"type":"tool_call","tool":"search","args":{"q":"Lake Champlain"}}');
assert.equal(search.tool, "search");
assert.equal(search.args.q, "Lake Champlain");
const fenced = toolCallOf('```json\n{"type":"tool_call","tool":"gather","args":{"url":"https://en.wikipedia.org/wiki/Lake_Champlain"}}\n```');
assert.equal(fenced.tool, "gather");
const loose = toolCallOf('{type:tool_call, tool:search, args: {"q":"Lake_Champlain"}}');
assert.equal(loose.tool, "search");
assert.equal(loose.args.q, "Lake_Champlain");

const shown = await settleReply(prose, 0, async () => { throw new Error("must not run"); });
assert.equal(shown.action, "finish");

const looked = await settleReply(JSON.stringify(search), 0, async () => ({ output: "1. Lake\nhttps://example.com", exit_code: 0 }));
assert.equal(looked.action, "follow");
assert.equal(looked.status, "Searching…");
assert.match(looked.wire, /Results:/);
assert.match(looked.fallback, /Lake/);
const echoed = await settleReply('{"type":"tool_result","tool":"search","output":"1. Lake","exit_code":0}', 1, async () => { throw new Error("must not run"); });
assert.equal(echoed.action, "show");
assert.equal(echoed.text, "1. Lake");

const clock = await settleReply('{"type":"tool_call","tool":"datetime","args":{}}', 0, runTool);
assert.equal(clock.action, "show");
assert.match(clock.text, /\d/);

// A tool call buried in a reply is still a call. A call that is still arriving is not text.
const wrapped = toolCallOf('I will look.\n{"type":"tool_call","tool":"search","args":{"q":"Lake Champlain"}}');
assert.equal(wrapped.tool, "search");
assert.equal(wrapped.args.q, "Lake Champlain");
const unclosed = toolCallOf('```json\n{"type":"tool_call","tool":"notes","args":{"op":"read","path":"a.md"}}');
assert.equal(unclosed.tool, "notes");
assert.equal(unclosed.args.path, "a.md");
const aliased = toolCallOf('{"type":"tool_call","name":"search","arguments":{"q":"tide"}}');
assert.equal(aliased.tool, "search");
assert.equal(aliased.args.q, "tide");
assert.equal(toolStream(prose).kind, "text");
assert.equal(toolStream("{not json").kind, "text");
assert.equal(toolStream("The set { is fine.").kind, "text");
assert.equal(toolStream('{"type":"tool_c').kind, "load");
assert.equal(toolStream('{"type":"tool_c').tool, "");
assert.equal(toolStream('{"type":"tool_call","tool":"s').tool, "");
assert.equal(toolStream('{"type":"tool_call","tool":"se').kind, "load");
assert.equal(toolStream('{"type":"tool_call","tool":"se').tool, "search");
const whole = toolStream('{"type":"tool_call","tool":"search","args":{"q":"x"}}');
assert.equal(whole.kind, "call");
assert.equal(whole.call.args.q, "x");
assert.equal(whole.lead, "");
const led = toolStream('Sure. {"type":"tool_call","tool":"gather","args":{"url":"https://example.com"}}');
assert.equal(led.kind, "call");
assert.equal(led.lead, "Sure.");
assert.equal(led.tool, "gather");
const pretty = toolStream('{\n  "type": "tool_call",\n  "tool": "tasks"');
assert.equal(pretty.kind, "load");
assert.equal(pretty.tool, "tasks");
assert.equal(toolCallOf("{type:tool_call, tool:notes}"), null);
assert.equal(noteUseful({ type: "tool_call", tool: "notes", args: {} }), false);
assert.equal(noteUseful(toolCallOf('{"type":"tool_call","tool":"notes","args":{"op":"list"}}')), true);
const drafted = toolCallOf('{type:tool_call, tool:notes, args:{op:write, path:"plans/today.md", text:"ship it"}}');
assert.equal(drafted.tool, "notes");
assert.equal(drafted.args.path, "plans/today.md");
assert.equal(drafted.args.text, "ship it");
assert.equal(toolStream("{type:tool_call, tool:notes}").kind, "call");
assert.equal(toolStream("{type:tool_call, tool:notes}").call.args.path, undefined);
assert.equal(noteWords({ args: {} }, ""), "");
assert.equal(noteWords({ args: { text: "..." } }, ""), "");
assert.equal(noteWords({ args: { text: "ship it" } }, "Sure.", "Done."), "Sure.\n\nship it\n\nDone.");
assert.equal(/[{]|tool_call/.test(draftText("Draft a short note.")), false);
assert.match(draftText("Draft a short note."), /short note/);
assert.equal(chatNote("Lake Champlain is long.", "Draft a short note."), "");
assert.equal(chatNote("{type:tool_call, tool:notes}", "Draft a short note."), draftText("Draft a short note."));
assert.equal(chatNote('{"type":"tool_call","tool":"notes"', "Draft a short note."), draftText("Draft a short note."));
assert.equal(chatNote('Hello.\n{"type":"tool_call","tool":"notes","args":{}} Thanks.', "Draft a short note."), "Hello.\n\nThanks.");
assert.equal(chatNote('{"type":"tool_call","tool":"notes","args":{"op":"write","path":"plans/today.md","text":"Meet at the dock."}}', "Draft a short note."), "Meet at the dock.");
assert.equal(chatNote('{"type":"tool_call","tool":"search","args":{"q":"x"}}', "Draft a short note."), "");
const cardAsk = "Write me a model card for this model";
const canvasCall = toolCallOf('{"type":"tool_call","tool":"canvas","args":{"op":"write","title":"card","text":"x"}}');
assert.equal(callAllowed(canvasCall, cardAsk, []), false);
assert.equal(callAllowed(canvasCall, "draw a diagram of the model", []), true);
assert.equal(callAllowed(canvasCall, cardAsk, ["canvas"]), true);
assert.equal(playedTool('This instruction was completed by using the "canvas" function of your internal system.\nThe response was:\n{"id":null}'), true);
assert.equal(playedTool("Hodge-3.5 is an auditable model you can run on this device."), false);
const ran = await settleReply('Look here {"type":"tool_call","tool":"search","args":{"q":"Lake Champlain"}}', 0, async () => ({ output: "1. Lake", exit_code: 0 }));
assert.equal(ran.action, "follow");
assert.equal(ran.status, "Searching…");

assert.equal(isPrivateAddress("127.0.0.1"), true);
assert.equal(isPrivateAddress("10.1.2.3"), true);
assert.equal(isPrivateAddress("169.254.169.254"), true);
assert.equal(isPrivateAddress("8.8.8.8"), false);
assert.equal(await allowedTarget("http://127.0.0.1/"), null);
assert.equal(await allowedTarget("http://169.254.169.254/latest/meta-data/"), null);
assert.equal(await allowedTarget("file:///C:/Windows"), null);
assert.equal(await allowedTarget("http://localhost/"), null);

const html = [
  '<a rel="nofollow" class="result__a" href="//duckduckgo.com/l/?uddg=https%3A%2F%2Fduckduckgo.com%2Fy.js%3Fad_domain%3Dads.example&amp;rut=1">Ad</a>',
  '<a class="result__snippet" href="//duckduckgo.com/l/?uddg=https%3A%2F%2Fduckduckgo.com%2Fy.js%3Fad_domain%3Dads.example">buy now</a>',
  '<a rel="nofollow" class="result__a" href="//duckduckgo.com/l/?uddg=https%3A%2F%2Fen.wikipedia.org%2Fwiki%2FLake_Champlain&amp;rut=1">Lake Champlain</a>',
  '<a class="result__snippet" href="//duckduckgo.com/l/?uddg=https%3A%2F%2Fen.wikipedia.org%2Fwiki%2FLake_Champlain">A lake in North America.</a>',
].join("\n");
const hits = parseDdg(html);
assert.equal(hits.length, 1);
assert.equal(hits[0].url, "https://en.wikipedia.org/wiki/Lake_Champlain");
assert.match(hits[0].snippet, /North America/);

const found = await searchWeb("Lake Champlain");
assert.match(found, /Champlain|wikipedia/i);
const local = await gatherPage("http://127.0.0.1/");
assert.equal(local.ok, false);
assert.match(local.output, /not available/);
const page = await gatherPage("https://en.wikipedia.org/wiki/Lake_Champlain");
assert.equal(page.ok, true);
assert.match(page.output, /Champlain/);
console.log("ext ok");
console.log(found.split("\n").slice(0, 4).join("\n"));
