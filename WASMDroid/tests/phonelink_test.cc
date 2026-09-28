#include "droid/catalog.h"
#include "droid/dispatch.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

static std::string Call(const char* api, const char* args) {
  char buf[1024];
  int rc = wasmdroid_call(api, args, buf, sizeof(buf));
  assert(rc == 0);
  return buf;
}

int main() {
  int n = 0;
  const WasmDroidApi* cat = wasmdroid_catalog(&n);
  assert(cat);
  assert(n == 14);
  assert(std::strcmp(cat[0].name, "PackageFamilyName") == 0);
  assert(std::strcmp(cat[9].name, "Sms") == 0);

  char buf[256];
  assert(std::strcmp(Call("PackageFamilyName", "").c_str(), "Microsoft.YourPhone_8wekyb3d8bbwe") == 0);
  assert(wasmdroid_call("Search", "hi", buf, sizeof(buf)) != 0);

  assert(Call("Open", "messages") == "ms-phone:messages");
  std::string sent = Call("Sms", "send\x1f" "+15551212\x1f" "dinner at 7");
  assert(sent.find("sms") != std::string::npos);
  assert(Call("Search", "dinner").find("dinner at 7") != std::string::npos);
  assert(Call("Read", "1").find("+15551212") != std::string::npos);
  assert(Call("Sms", "list").find("dinner at 7") != std::string::npos);

  Call("Write", "notification\x1f" "Messages\x1f" "dinner at 7");
  Call("Write", "photo\x1f" "camera\x1f" "img-1");
  Call("Write", "call\x1f" "+15551212\x1f" "missed");
  Call("Write", "app\x1f" "com.android.mms\x1f" "Messages");

  std::string mon = Call("Monitor", "");
  assert(mon.find("dinner at 7") != std::string::npos);
  assert(mon.find("notification") != std::string::npos);
  assert(Call("Monitor", "sms").empty());
  assert(Call("Photos", "").find("img-1") != std::string::npos);
  assert(Call("Notifications", "").find("Messages") != std::string::npos);
  assert(Call("Calls", "").find("missed") != std::string::npos);
  assert(Call("Apps", "").find("com.android.mms") != std::string::npos);
  assert(Call("Search", "15551212").find("missed") != std::string::npos);

  assert(wasmdroid_call("getpid", "", buf, sizeof(buf)) != 0);
  std::cout << "phonelink ok\n";
  return 0;
}
