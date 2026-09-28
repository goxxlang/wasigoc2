# WASMNix

WSL, in this module.

`gocvm.Call("linux"|"wsl"|"nix", …)` hits `wsl_call`. A distribution can be installed, set as default, started, stopped, exported, imported, and unregistered. `Exec` runs in that distro. `Read` and `Write` are its files, including `/mnt/<drive>`. `Path` is `wslpath`. `Config` is `.wslconfig` (memory, processors, networking mode, WSLg, localhost forwarding). `Configure` is `/etc/wsl.conf` (interop, systemd, automount). `LaunchWin32` is interop.

```
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```
