# WASMDroid

Phone Link. The Windows app YourPhone, in this module.

| | |
| --- | --- |
| Package family | `Microsoft.YourPhone_8wekyb3d8bbwe` |
| AUMID | `Microsoft.YourPhone_8wekyb3d8bbwe!App` |
| Protocol | `ms-phone:` |

`gocvm.Call("phonelink", …)` and `gocvm.Call("android", …)` both hit
`phonelink_call`. After `Open`, the link can `Search`, `Read`, `Write`,
and `Monitor` SMS, notifications, photos, calls, and apps. `Sms` sends
and lists messages. `Status` is `unlinked` until `Open`.

```
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```
