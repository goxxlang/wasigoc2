#include "gocos/dispatch.h"
#include "gocos/unil.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#if defined(WASMGOCOS_HAS_JITRT)
#include "src/runtime/unil.h"
#endif

// Fixtures are packed by tests/data/pack_unil.py, not by the kernel.
static std::vector<uint8_t> Slurp(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

static std::string Data(const char* name) { return std::string(GOCOS_TEST_DATA) + "/" + name; }

#if defined(WASMGOCOS_HAS_JITRT)
static std::string Ctr(const std::string& args) {
  static char buf[16384];
  buf[0] = 0;
  wasmgocos_call("Ctr", args.c_str(), buf, sizeof(buf));
  return buf;
}

static std::string Region(unsigned pid, const char* region) {
  return Ctr(std::string("read") + '\x1f' + std::to_string(pid) + '\x1f' + region);
}

static unsigned Pid(const std::string& line) {
  unsigned pid = 0;
  assert(std::sscanf(line.c_str(), "ctr pid=%u", &pid) == 1);
  return pid;
}
#endif

#if defined(WASMGOCOS_HAS_JITRT)
static const char kPersisted[] = "state written by the first boot, longer than the service it replaced";

// The first boot: a container whose region grew, saved to `path`.
static int SaveBoot(const std::string& path) {
  char boot[4096];
  assert(wasmgocos_call("Boot", "", boot, sizeof(boot)) == 0);
  std::string line = Ctr(std::string("load") + '\x1f' + "persist");
  unsigned pid = Pid(line);
  char reply[256];
  assert(wasmgocos_call("Vfs",
                        (std::string("write") + '\x1f' + "/ctr/persist/service" + '\x1f' + kPersisted)
                            .c_str(),
                        reply, sizeof(reply)) == 0);
  assert(Region(pid, "service") == kPersisted);
  std::string saved = Ctr(std::string("save") + '\x1f' + "persist" + '\x1f' + path);
  std::printf("%s\n", saved.c_str());
  assert(saved.rfind("save " + path + " name=persist regions=5 ", 0) == 0);
  return 0;
}

// The next boot: a fresh process opens what the first one saved.
static int ReBoot(const std::string& path) {
  char boot[4096];
  assert(wasmgocos_call("Boot", "", boot, sizeof(boot)) == 0);
  assert(Ctr("list") == "ctr=0");
  // `open` names a container after the file unless told; the region keeps
  // the name it was saved under.
  std::string line = Ctr(std::string("open") + '\x1f' + path + '\x1f' + "persist");
  std::printf("%s\n", line.c_str());
  std::fflush(stdout);
  assert(line.find(" name=persist ") != std::string::npos);
  assert(line.find(" regions=5 ") != std::string::npos);
  unsigned pid = Pid(line);
  assert(Region(pid, "service") == kPersisted);
  assert(Region(pid, "name") == "persist");
  assert(Region(pid, "result") == "3");
  assert(Ctr(std::string("call") + '\x1f' + std::to_string(pid) + '\x1f' + "gocos" + '\x1f' + "Rc") ==
         "0");
  // Consumed, so this check cannot pass on a file an earlier run left.
  assert(std::remove(path.c_str()) == 0);
  return 0;
}
#endif

int main(int argc, char** argv) {
  using namespace wasmgocos;
  std::string why;
  if (argc == 3) {
#if defined(WASMGOCOS_HAS_JITRT)
    if (std::strcmp(argv[1], "save") == 0) return SaveBoot(argv[2]);
    if (std::strcmp(argv[1], "reboot") == 0) return ReBoot(argv[2]);
#endif
    return 0;
  }

  // The kernel's own gzip writer, read back by its inflater: empty, short,
  // a run past the longest match, text, and bytes with no repeats.
  {
    std::vector<std::vector<uint8_t>> inputs;
    inputs.push_back({});
    inputs.push_back({'x'});
    inputs.push_back(std::vector<uint8_t>(1000, 'a'));
    std::string text;
    for (int i = 0; i < 3000; ++i) text += "line " + std::to_string(i % 97) + ": the cage object\n";
    inputs.push_back(std::vector<uint8_t>(text.begin(), text.end()));
    std::vector<uint8_t> noise(70000);
    uint32_t x = 2463534242u;
    for (uint8_t& b : noise) {
      x ^= x << 13;
      x ^= x >> 17;
      x ^= x << 5;
      b = static_cast<uint8_t>(x);
    }
    // A copy 40000 bytes back is past the window and has to be literals.
    std::copy(noise.begin(), noise.begin() + 5000, noise.begin() + 40000);
    inputs.push_back(noise);
    for (const std::vector<uint8_t>& in : inputs) {
      std::vector<uint8_t> gz = unil_gzip(in.data(), in.size());
      std::vector<uint8_t> back;
      bool ok = unil_gunzip(gz.data(), gz.size(), &back, &why);
      if (!ok) std::fprintf(stderr, "gzip %zu: %s\n", in.size(), why.c_str());
      assert(ok && back == in);
    }
    std::vector<uint8_t> gz = unil_gzip(inputs[3].data(), inputs[3].size());
    assert(gz.size() * 10 < inputs[3].size());
  }

  // unil_pack's bundle is the gzip form, and the index keeps kind and flags.
  {
    const char* js = "'packed ' + 7";
    const char* notes = "notes notes notes notes";
    std::vector<UnilPart> parts = {
        {"app.js", "text/javascript", 1, 2, reinterpret_cast<const uint8_t*>(js), std::strlen(js)},
        {"empty", "", 0, 0, nullptr, 0},
        {"lib/notes.md", "text/markdown", 12, 0, reinterpret_cast<const uint8_t*>(notes),
         std::strlen(notes)},
    };
    std::vector<uint8_t> packed = unil_pack(parts);
    assert(packed.size() > 32 && packed[6] == 1);
    std::vector<uint8_t> plain;
    assert(unil_plain(packed.data(), packed.size(), &plain, &why));
#if defined(WASMGOCOS_HAS_JITRT)
    wasmturbo::UnilStore store = wasmturbo::UnilStore::Parse(plain.data(), plain.size());
    assert(store.regions().size() == 3);
    const wasmturbo::UnilRegion* app = store.Find("app.js");
    assert(app != nullptr && app->kind == 1 && app->flags == 2 && app->mime == "text/javascript");
    const wasmturbo::UnilRegion* md = store.Find("lib/notes.md");
    assert(md != nullptr && md->kind == 12 && md->size == std::strlen(notes));
    assert(std::memcmp(store.payload().data() + md->offset, notes, md->size) == 0);
    assert(store.Find("empty") != nullptr && store.Find("empty")->size == 0);
#endif
    parts[0].name.clear();
    assert(unil_pack(parts).empty());
  }

  // A fixed-Huffman member (python gzip, level 9).
  const uint8_t fixed[] = {0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x0a, 0xcb,
                           0x48, 0xcd, 0xc9, 0xc9, 0x57, 0xc8, 0x40, 0x22, 0xd3, 0xf3, 0x93,
                           0xf3, 0x8b, 0x01, 0xe7, 0x6e, 0xa0, 0x54, 0x17, 0x00, 0x00, 0x00};
  std::vector<uint8_t> out;
  assert(unil_gunzip(fixed, sizeof(fixed), &out, &why));
  assert(std::string(out.begin(), out.end()) == "hello hello hello gocos");
  std::vector<uint8_t> bent(fixed, fixed + sizeof(fixed));
  bent[sizeof(fixed) - 8] ^= 1;
  assert(!unil_gunzip(bent.data(), bent.size(), &out, &why) && why == "gzip crc");
  for (size_t n = 0; n < sizeof(fixed); ++n) assert(!unil_gunzip(fixed, n, &out, &why));

  // Dynamic, plain, and stored payloads all come back as the plain form.
  for (const char* name : {"apps.unil", "plain.unil", "stored.unil"}) {
    std::vector<uint8_t> file = Slurp(Data(name));
    assert(!file.empty());
    std::vector<uint8_t> plain;
    bool ok = unil_plain(file.data(), file.size(), &plain, &why);
    if (!ok) std::fprintf(stderr, "%s: %s\n", name, why.c_str());
    assert(ok);
    assert(plain[6] == 0 && plain[7] == 0);
#if defined(WASMGOCOS_HAS_JITRT)
    wasmturbo::UnilStore store = wasmturbo::UnilStore::Parse(plain.data(), plain.size());
    assert(store.Find("app.js") != nullptr);
#endif
  }

  // A damaged bundle is a reply, never an abort: every truncation and every
  // single-bit flip. Whatever still validates has to parse.
  std::vector<uint8_t> apps = Slurp(Data("apps.unil"));
  for (size_t n = 0; n < apps.size(); ++n) {
    std::vector<uint8_t> plain;
    assert(!unil_plain(apps.data(), n, &plain, &why));
  }
  int accepted = 0;
  for (size_t i = 0; i < apps.size(); ++i) {
    for (int bit = 0; bit < 8; ++bit) {
      std::vector<uint8_t> flip = apps;
      flip[i] ^= static_cast<uint8_t>(1u << bit);
      std::vector<uint8_t> plain;
      if (!unil_plain(flip.data(), flip.size(), &plain, &why)) continue;
      ++accepted;
#if defined(WASMGOCOS_HAS_JITRT)
      wasmturbo::UnilStore::Parse(plain.data(), plain.size());
#endif
    }
  }
  // Only name, mime, and kind bytes carry no checksum.
  assert(accepted > 0 && accepted < static_cast<int>(apps.size()));

#if defined(WASMGOCOS_HAS_JITRT)
  char boot[4096];
  assert(wasmgocos_call("Boot", "", boot, sizeof(boot)) == 0);

  // Two scripts share one context and run in index order.
  std::string line = Ctr(std::string("open") + '\x1f' + Data("apps.unil"));
  if (line.rfind("ctr pid=", 0) != 0) std::fprintf(stderr, "open: %s\n", line.c_str());
  assert(line.find(" name=apps ") != std::string::npos);
  assert(line.find(" regions=4 ") != std::string::npos);
  unsigned apps_pid = Pid(line);
  assert(Region(apps_pid, "result") == "2,4,6");
  assert(Region(apps_pid, "service") == "apps from outside the kernel");
  assert(Region(apps_pid, "notes.md").rfind("line 0: GocOS containers", 0) == 0);
  assert(Region(apps_pid, "name") == "apps");

  // The kernel's frame is in the bundle, so gocvm.Call reaches gocos.
  assert(Ctr(std::string("call") + '\x1f' + std::to_string(apps_pid) + '\x1f' + "gocos" + '\x1f' +
             "Rc") == "0");

  // The origin filesystem has the bundle and each top-level region.
  char ls[512];
  assert(wasmgocos_call("Vfs", (std::string("ls") + '\x1f' + "/ctr/apps").c_str(), ls,
                        sizeof(ls)) == 0);
  for (const char* region : {"service\n", "app.js\n", "notes.md\n", "gocvm.Call\n", "result\n"})
    assert(std::strstr(ls, region) != nullptr);
  char head[8];
  assert(wasmgocos_call("Vfs", (std::string("read") + '\x1f' + "/ctr/apps.unil").c_str(), head,
                        sizeof(head)) == 0);
  assert(std::strncmp(head, "UNIL", 4) == 0);

  unsigned plain_pid = Pid(Ctr(std::string("open") + '\x1f' + Data("plain.unil")));
  assert(Region(plain_pid, "result") == "plain 42");
  unsigned stored_pid = Pid(Ctr(std::string("open") + '\x1f' + Data("stored.unil") + '\x1f' + "rev"));
  assert(Region(stored_pid, "result") == "stored cba");
  assert(Ctr("list").find(" name=rev ") != std::string::npos);

  // The session shell opens the same way.
  char sh[1024];
  assert(wasmgocos_call("Sh", ("ctr open " + Data("plain.unil") + " sh42").c_str(), sh,
                        sizeof(sh)) == 0);
  assert(std::strstr(sh, " name=sh42 ") != nullptr);

  // A grown region is saved at its new size and opens as another container.
  const char* grown = "apps from outside the kernel, grown before the save";
  char wrote[256];
  assert(wasmgocos_call("Vfs",
                        (std::string("write") + '\x1f' + "/ctr/apps/service" + '\x1f' + grown).c_str(),
                        wrote, sizeof(wrote)) == 0);
  std::string saved_path = Data("apps.saved.tmp");
  std::string saved = Ctr(std::string("save") + '\x1f' + "apps" + '\x1f' + saved_path);
  if (saved.rfind("save ", 0) != 0) std::fprintf(stderr, "save: %s\n", saved.c_str());
  assert(saved.find(" name=apps regions=7 ") != std::string::npos);
  std::vector<uint8_t> saved_file = Slurp(saved_path);
  assert(saved_file.size() > 32 && saved_file[6] == 1);
  assert(saved_file.size() < apps.size() + 512);
  unsigned again = Pid(Ctr(std::string("open") + '\x1f' + saved_path + '\x1f' + "apps2"));
  assert(Region(again, "service") == grown);
  assert(Region(again, "notes.md") == Region(apps_pid, "notes.md"));
  assert(Region(again, "lib/two.js") == "var two = 2; two");
  assert(Region(again, "result") == "2,4,6");
  assert(Region(again, "name") == "apps");
  assert(wasmgocos_call("Sh", ("ctr save apps2 " + saved_path).c_str(), sh, sizeof(sh)) == 0);
  assert(std::strncmp(sh, "save ", 5) == 0);
  assert(std::strstr(sh, " name=apps2 regions=7 ") != nullptr);
  assert(Region(Pid(Ctr(std::string("open") + '\x1f' + saved_path + '\x1f' + "apps3")), "service") ==
         grown);
  std::remove(saved_path.c_str());
  assert(Ctr(std::string("save") + '\x1f' + "nobody" + '\x1f' + saved_path) == "ctr miss");
  assert(Ctr(std::string("save") + '\x1f' + "apps" + '\x1f' + Data("no/such/dir/x.unil"))
             .rfind("error: ctr: save", 0) == 0);
  assert(Slurp(saved_path).empty());

  // Refusals leave the kernel up.
  char miss[256];
  assert(wasmgocos_call("Ctr", (std::string("open") + '\x1f' + Data("none.unil")).c_str(), miss,
                        sizeof(miss)) != 0);
  assert(std::strstr(miss, "error: ctr: open") != nullptr);
  std::string torn = Data("torn.unil.tmp");
  {
    std::ofstream o(torn, std::ios::binary);
    o.write(reinterpret_cast<const char*>(apps.data()), static_cast<std::streamsize>(apps.size() / 2));
  }
  assert(wasmgocos_call("Ctr", (std::string("open") + '\x1f' + torn).c_str(), miss, sizeof(miss)) != 0);
  assert(std::strstr(miss, "error: ctr: unil") != nullptr);
  std::remove(torn.c_str());

  // A bundle ~/WASMUniLoader packed, when that tree is on this machine.
  std::string demo = std::string(GOCOS_TEST_DATA) + "/../../../WASMUniLoader/web/demo.unil";
  if (!Slurp(demo).empty()) {
    line = Ctr(std::string("open") + '\x1f' + demo + '\x1f' + "uldemo");
    if (line.rfind("ctr pid=", 0) != 0) std::fprintf(stderr, "demo: %s\n", line.c_str());
    assert(line.find(" regions=20 ") != std::string::npos);
    unsigned demo_pid = Pid(line);
    assert(Region(demo_pid, "dashboard.json").find("UniLoader sandbox console") != std::string::npos);
    assert(Region(demo_pid, "hello.gml").rfind("div.unil-hello#demo(", 0) == 0);
    std::printf("demo.unil: %s\n", line.c_str());
  }
#endif
  std::printf("unil_test ok (%d single-bit flips still valid)\n", accepted);
  return 0;
}
