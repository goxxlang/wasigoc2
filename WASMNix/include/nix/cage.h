#ifndef WASMNIX_INCLUDE_NIX_CAGE_H_
#define WASMNIX_INCLUDE_NIX_CAGE_H_

// Linux answers for a distro that lives in the cage. Same names as
// wasmnix_call for the WSL Linux-side tools (uname, wslpath, echo),
// served by posix_host.hpp. This entry does not CreateProcessW(wsl.exe)
// and does not popen a host shell.
#ifdef __cplusplus
extern "C" {
#endif

int wasmnix_cage_call(const char* api, const char* args, char* out, unsigned cap);

#ifdef __cplusplus
}
#endif

#endif  // WASMNIX_INCLUDE_NIX_CAGE_H_
