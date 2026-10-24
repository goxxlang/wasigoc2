#ifndef WASMNIX_INCLUDE_NIX_DRVFS_H_
#define WASMNIX_INCLUDE_NIX_DRVFS_H_

// DrvFs path map from microsoft/WSL wslpath: C:\foo <-> /mnt/c/foo.
// -m emits a forward-slash Windows path. No wsl.exe and no wslpath binary.
#include <string>

namespace wasmnix {

inline std::string drvfs_translate(const std::string& flags, const std::string& path) {
  const bool to_unix = flags.find("-u") != std::string::npos;
  const bool to_mixed = flags.find("-m") != std::string::npos;
  const bool to_win = flags.find("-w") != std::string::npos || to_mixed;
  const bool drive = path.size() >= 2 &&
                     ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
                     path[1] == ':';
  const bool mnt = path.rfind("/mnt/", 0) == 0 && path.size() > 6;
  if (!to_win && drive) {
    char letter = path[0];
    if (letter >= 'A' && letter <= 'Z') letter = static_cast<char>(letter - 'A' + 'a');
    std::string out = "/mnt/";
    out.push_back(letter);
    for (size_t i = 2; i < path.size(); ++i) out.push_back(path[i] == '\\' ? '/' : path[i]);
    return out;
  }
  if (!to_unix && mnt) {
    char letter = path[5];
    if (letter >= 'a' && letter <= 'z') letter = static_cast<char>(letter - 'a' + 'A');
    std::string out;
    out.push_back(letter);
    out.push_back(':');
    const char sep = to_mixed ? '/' : '\\';
    for (size_t i = 6; i < path.size(); ++i) out.push_back(path[i] == '/' ? sep : path[i]);
    return out;
  }
  if (to_mixed && drive) {
    std::string out = path;
    for (char& c : out)
      if (c == '\\') c = '/';
    return out;
  }
  return path;
}

}  // namespace wasmnix

#endif  // WASMNIX_INCLUDE_NIX_DRVFS_H_
