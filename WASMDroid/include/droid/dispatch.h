#ifndef WASMDROID_INCLUDE_DROID_DISPATCH_H_
#define WASMDROID_INCLUDE_DROID_DISPATCH_H_

// One call. Phone Link, the Windows app.
#ifdef __cplusplus
extern "C" {
#endif

// api: Search, Read, Write, Monitor, Sms, Photos, Notifications, Calls, Apps.
// args: UTF-8, 0x1F-separated (may be empty).
// out: UTF-8 reply, NUL-terminated when cap > 0.
// returns 0 on success; -1 for an unknown API.
int wasmdroid_call(const char* api, const char* args, char* out, unsigned cap);

#ifdef __cplusplus
}
#endif

#endif  // WASMDROID_INCLUDE_DROID_DISPATCH_H_
