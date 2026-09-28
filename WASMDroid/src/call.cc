#include "droid/phonelink.hpp"

#include <string>

extern "C" int wasmdroid_call(const char* api, const char* args, char* out, unsigned cap) {
  std::string reply = wasmdroid::phonelink_call(api, args);
  return wasmdroid::fill_out(out, cap, reply);
}
