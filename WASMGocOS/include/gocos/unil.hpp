#ifndef WASMGOCOS_INCLUDE_GOCOS_UNIL_HPP_
#define WASMGOCOS_INCLUDE_GOCOS_UNIL_HPP_

// A `.unil` bundle packed outside this process. ~/WASMUniLoader writes the
// payload as one gzip stream (header flag bit 0); the engine's UnilStore
// reads only the plain form and aborts on a bad container. These check
// every field first, so a bad file is a reply and not a crash.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace wasmgocos {

// CRC32-IEEE, the checksum the container uses for its header and regions.
uint32_t unil_crc32(const uint8_t* data, size_t n);

// RFC 1952 member around an RFC 1951 stream. Checks the trailer's CRC32 and
// size. False with `why` set on anything else.
bool unil_gunzip(const uint8_t* data, size_t n, std::vector<uint8_t>* out, std::string* why);

// The same container with an uncompressed payload: every offset, size, and
// region CRC checked against the inflated bytes. The result is what
// wasmturbo::UnilStore::Parse accepts.
bool unil_plain(const uint8_t* data, size_t n, std::vector<uint8_t>* plain, std::string* why);

// RFC 1952 member around one fixed-Huffman RFC 1951 block with LZ77
// matches. unil_gunzip reads it back.
std::vector<uint8_t> unil_gzip(const uint8_t* data, size_t n);

// One region of a bundle the kernel writes. `data` is borrowed.
struct UnilPart {
  std::string name;
  std::string mime;
  uint8_t kind = 0;
  uint8_t flags = 0;
  const uint8_t* data = nullptr;
  size_t size = 0;
};

// ~/WASMUniLoader's gzip form: header, index, one gzip payload, regions in
// the order given. Empty when a name or mime does not fit its u16 length.
std::vector<uint8_t> unil_pack(const std::vector<UnilPart>& parts);

}  // namespace wasmgocos

#endif  // WASMGOCOS_INCLUDE_GOCOS_UNIL_HPP_
