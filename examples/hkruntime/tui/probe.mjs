import { spawn } from "node:child_process";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";

const here = dirname(fileURLToPath(import.meta.url));
const child = spawn(process.execPath, ["hodge.mjs", "C:\\Users\\grego\\models\\dev\\hodge-4"], {
  cwd: here,
  stdio: ["pipe", "pipe", "pipe"],
});
let err = "";
let sent = false;
child.stderr.on("data", (d) => {
  const t = d.toString();
  process.stderr.write(t);
  err += t;
  if (!sent && err.includes("type a message")) {
    sent = true;
    child.stdin.write("What is the capital of France?\n");
  }
});
child.stdout.on("data", (d) => process.stdout.write("OUT " + JSON.stringify(d.toString()) + "\n"));
child.on("exit", (code) => {
  console.error("exit", code, "sent", sent);
  process.exit(code || 0);
});
setTimeout(() => {
  console.error("timeout sent", sent);
  child.kill();
  process.exit(1);
}, 120000);
