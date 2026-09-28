#include "droid/catalog.h"

// Phone Link (YourPhone): search, read, write, and monitor the linked phone.
static const WasmDroidApi kApis[] = {
    {"Windows.PhoneLink", "YourPhone", "PackageFamilyName"},
    {"Windows.PhoneLink", "YourPhone", "Aumid"},
    {"Windows.PhoneLink", "YourPhone", "Protocol"},
    {"Windows.PhoneLink", "YourPhone", "Open"},
    {"Windows.PhoneLink", "YourPhone", "Status"},
    {"Windows.PhoneLink", "YourPhone", "Search"},
    {"Windows.PhoneLink", "YourPhone", "Read"},
    {"Windows.PhoneLink", "YourPhone", "Write"},
    {"Windows.PhoneLink", "YourPhone", "Monitor"},
    {"Windows.PhoneLink", "YourPhone", "Sms"},
    {"Windows.PhoneLink", "YourPhone", "Photos"},
    {"Windows.PhoneLink", "YourPhone", "Notifications"},
    {"Windows.PhoneLink", "YourPhone", "Calls"},
    {"Windows.PhoneLink", "YourPhone", "Apps"},
};

const WasmDroidApi* wasmdroid_catalog(int* count) {
  if (count) *count = (int)(sizeof(kApis) / sizeof(kApis[0]));
  return kApis;
}
