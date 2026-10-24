// Pack or unpack a safetensors file with the BNZ2 codec.
//   node web/pack.mjs c model.safetensors model.bnz
//   node web/pack.mjs d model.bnz model.safetensors
import fs from "node:fs";
import { compressSafetensors, decompressSafetensors } from "./bnz.js";

const [cmd, inn, out] = process.argv.slice(2);
if ((cmd !== "c" && cmd !== "d") || !inn || !out) {
  process.stderr.write("usage: node pack.mjs c|d in out\n");
  process.exit(2);
}
const data = new Uint8Array(fs.readFileSync(inn));
const result = cmd === "c" ? compressSafetensors(data) : decompressSafetensors(data);
fs.writeFileSync(out, result);
process.stdout.write(cmd + " " + data.length + " -> " + result.length + "\n");
