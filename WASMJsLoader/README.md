# WASMJsLoader

Browser load / instantiate / run / call for compiled `.wasm` (wasigocvm
guests included). The guest **Calls** this hop. It does not rewrite
`~/WASMWin32`, `~/WASMLoader`, or the guest.

Same command surface as `~/WASMLoader` / wasitime:

```
inspect  <bytes>          imports + exports
run      <bytes>          instantiate, `_start` (or `_initialize`)
call     <bytes> <export> [i32...]
```

Win32 / gocvm stay **inside** the module (`wasi_host.hpp` + catalog).
This file only supplies the host imports those modules already declare:
`wasi_snapshot_preview1` (and `wasi_unstable` aliases) plus `env.log` /
`env.abort` — the same `env` names `~/WASMLoader` `loader/host.go`
exports. Packed string imports (`alloc` + JSON envelope) are optional
callbacks a guest can register; they are not a second Win32.

## Use

wasitime is `~/WASMLoader` + `~/WASMSafeSpace` (JIT) — cranelift. This hop
is the browser instantiate of that same guest. Serve is Go:

```
go run ./cmd/jsloader
```

GocOS Command Prompt (xterm.js, wasigocvm guest, no host bridge):

```
wasigocvm.bat examples\cmdterm\main.go -o examples\cmdterm\cmdterm.wasm
go run ./cmd/jsloader
```

Open `http://127.0.0.1:8792/cmd.html`. Drop a `.wasm`, inspect, run, or call an export. From JS:

```js
import { inspect, instantiate, run, call } from "./loader.js";

const bytes = await fetch("hello.wasm").then((r) => r.arrayBuffer());
console.log(inspect(bytes));
const inst = await instantiate(bytes, { args: ["hello.wasm"], stdout: console.log });
await run(inst);
```

## What this is not

- Not wazero / Wasmtime in the page (`~/WASMLoader` webui still POSTs
  to its Go server; this hop instantiates in the browser).
- Not a rewrite of WASMWin32. `\x1f` protocol stays in the guest.
- Not ART / PE payload execution — those stay data in
  their mappers. This runs the wasm already compiled.
