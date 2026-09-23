# Guest PE entry point writes a host file

## Summary

A purpose-built Windows portable executable, started by the in-guest `CreateProcessW` path, executed its entry point. The instructions addressed a path and a payload stored in the image. The file write and the Windows environment both stay inside the one wasigocvm module. The same module was then run in Electron and Chrome at `http://127.0.0.1:8765/index.html`. On that page the WebAssembly executed in the browser. The HTTP server only delivered the bytes.

## What was trusted, and what the image controlled

The runtime is trusted: image resolution, the PE mapper, the virtual-processor runner, and the k32 file image inside the wasigocvm module.

The image is the controlled input. Its entry point and the record those instructions address choose the path and the bytes. The write follows from executing that entry, not from the file name `proof.exe`.

## Image

`proof.exe` is an i386 MZ image, packed with `calc.exe` in the UniLoader system bundle and loaded when `CreateProcessW` asks for `proof.exe`.

Entry point:

```
mov eax, 0x1080
out 0xE9, al
hlt
```

RVA `0x1080` is a length-prefixed record:

- path: `C:\Users\grego\go++\fs\pe-proof.txt`
- body: `pe-executed` plus a newline (12 bytes)

## What the product did

WASMGocOS already hops files and the process environment through k32. That path is what runs.

`CreateProcessW` resolved `proof.exe` from the bundle, mapped the PE, and ran `AddressOfEntryPoint` on the virtual processor. The runner executes a straight-line sequence: `mov` of an 8-bit or 32-bit immediate, `in`/`out` (immediate port or `DX`), and `hlt` / `ret` / `int3`.

`out` to port `0xE9` stores the path and body in k32's file image, the same image `GocKrnl.WriteFile` uses. `CreateFileW`, `WriteFile`, and `ReadFile` open that image when libc cannot create the path. Stdout is only stdout.

`GetEnvironmentStringsW` and `GetEnvironmentVariableW` read a Windows environment the module owns: `SystemRoot`, `WINDIR`, `PATH`, `ComSpec`, `PATHEXT`, `USERPROFILE`, `TEMP`, and the rest. A value already in the process is kept. `GOCVM_NT_VERSION` is set only when the process actually has it. WASMGocOS sees this block on the k32 hop.

`go++\WASMWin32` is a real directory, not a link to `~\WASMWin32`. The in-module file image and the Windows environment are in both trees.

## Evidence

wasitime, then WASMJsLoader through the same `loader.js` the page uses:

```
pid=10000 proc=256 thr=256
image=proof.exe win32=CreateProcessW hwnd=1922 mod=1796 wrote=C:\Users\grego\go++\fs\pe-proof.txt bytes=12 rc=0
exit 0
```

`C:\Users\grego\go++\fs\pe-proof.txt` contains:

```
pe-executed
```

Browser run: Electron and Chrome, page `http://127.0.0.1:8765/index.html` (WASMJsLoader). The module is `examples/proofshow/proofshow.wasm`. The page calls `WebAssembly.instantiate` in the client (`loader.js`). Nothing on the server runs the guest. A second static serve of the same page is at `http://127.0.0.1:8792/`, with the module also at `/proofshow.wasm`.

## Scope

The runner executes that straight-line opcode set. `calc.exe` on this path demonstrates image mapping and window creation (`hwnd`, module handle). `proof.exe` is the case where the entry point runs and the bytes land in the k32 file image.
