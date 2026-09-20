# wasip2 (retired as product)

Do not target stock wasip2 with flag packs.

The web-native machine is **wasigocvm**: own sysroot, own libc, own
runtime (`wasitime`). Architecture: [architecture.md](architecture.md).
Versus Bytecode Alliance: [wasigocvm.md](wasigocvm.md).

`compile.bat --wasip2` and `legacy.bat --wasip2` exit with a redirect.
A wasip2 clang wrapper may
still be used under the hood until `toolchain/` has our own triple —
that is an implementation detail, not the ABI or the docs story.
