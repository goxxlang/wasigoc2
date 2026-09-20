# wasitime

**wasitime is our engine** — the same *place* Cranelift occupies in
Wasmtime, not a Cranelift port.

Wasmtime: CLI (`wasmtime run` / `call` / `inspect`) → **Cranelift**
(or Winch) compiles wasm to host ISA, then jumps.

wasitime: CLI (`wasitime run` / `call` / `inspect`) → **wazeropkg**
compiles the WASM 2 fork to an `Op` stream in the WASMSafeSpace cage,
names it on CPT/CHPT, then interprets. No Cranelift, no LLVM, no
wazevo.

Bytecode: [wasm2-fork.md](wasm2-fork.md). Host ABI: [host-abi.md](host-abi.md).

## CLI (wasmtime-shaped)

```
wasitime inspect <file.wasm>
wasitime run     <file.wasm> [--] [argv...]
wasitime call    <file.wasm> <export> [args...]
wasitime link    name=file.wasm ... [--call] module.export [args...]
wasitime example <add|double|hello|plugin>
wasitime <file.wasm>                 same as run
```

`wasitime.bat` / `wasitime.sh` build `examples/wasitime` with `goclang++`
(`wasitime.exe`). The same program as a wasigocvm module is
`wasitime.wasm`.

## Like Cranelift — same slot, different machine

| | Wasmtime + Cranelift | wasitime |
| --- | --- | --- |
| Input | core wasm | core wasm (this fork; outer wrapper discarded) |
| Compile | Cranelift ISA → host code | wasm opcodes → `Op[]` in the cage |
| Code handle | native function pointer | CPT entrypoint (`funcidx+1`) + interned CHPT tag |
| Run | jump to compiled ISA | interpreter loop on `Op` |
| Memory | engine linear memory | cage pages; GIA = compressed `ptr-Base` |
| Tables / globals | engine arrays | cage objects, same grow/relocate as cppgc |
| GC | none / WASM GC | Oilpan; `0xfb` rejected |
| Host ABI | WASI / WIT | leftover stdio traps; Win32/Nix/Droid in the **guest** |
| Jobs | Cranelift compile parallelism | cppgc `PostJob` (blocking runs on post) |

Cranelift is a **native codegen** backend. wasitime is an **interpreter
backend** for the same wasm binary format (version 1, WASM 2 opcodes).
Both sit under a wasmtime-shaped CLI and turn a module into something
the process can execute.

```
.wasm
  │
  ▼
wasmbin.GuestCore          peel wrapper → core
  │
  ▼
wazeropkg.CompileModule    Oilpan job per function
  │                        code bytes → cage, CPT.Register
  │                        functype interned, CHPT/TPT named
  │
  ▼
Instantiate                memory / table / globals in the cage
  │
  ▼
run / call                 execFn walks Op[]; v128 is a CHPT handle
```

`invoke` checks CPT `GetEntrypoint`. A forged handle does not run.

## Packages

| Package | Job |
| --- | --- |
| `examples/wasitime` | CLI |
| `examples/wasmloaderpkg` | load / run / call / link |
| `examples/wasmbinpkg` | decode |
| `examples/wazeropkg` | compile + interpret (the Cranelift-slot) |
| `examples/safespacepkg` | cage, GIA, CPT/EPT/TPT |
| `examples/v8bindpkg` | CHPT |

Not a vendor dump of tetratelabs/wazero. Not w2g. Not Wasmtime.

## Run

```bash
compile.bat examples\hello\hello.go -o examples\hello\hello.wasm
wasitime inspect examples\hello\hello.wasm
wasitime run     examples\hello\hello.wasm
wasitime call    examples\wasitime\add.wasm add 2 3
```

`inspect` prints core magic `00 61 73 6d 01 00 00 00`. `run` of hello
prints `hello, wasi`.
