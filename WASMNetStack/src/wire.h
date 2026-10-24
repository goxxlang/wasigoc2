#ifndef WNS_WIRE_H_
#define WNS_WIRE_H_

#include <cstdint>
#include <vector>

namespace wns {
namespace wire {

inline void PutU8(std::vector<uint8_t>* o, uint8_t v) { o->push_back(v); }

inline void PutBe16(std::vector<uint8_t>* o, uint16_t v) {
  o->push_back(uint8_t(v >> 8));
  o->push_back(uint8_t(v));
}

inline void PutBe32(std::vector<uint8_t>* o, uint32_t v) {
  o->push_back(uint8_t(v >> 24));
  o->push_back(uint8_t(v >> 16));
  o->push_back(uint8_t(v >> 8));
  o->push_back(uint8_t(v));
}

inline void PutBe64(std::vector<uint8_t>* o, uint64_t v) {
  PutBe32(o, uint32_t(v >> 32));
  PutBe32(o, uint32_t(v));
}

inline uint8_t GetU8(const uint8_t* p) { return p[0]; }

inline uint16_t GetBe16(const uint8_t* p) {
  return uint16_t((uint16_t(p[0]) << 8) | uint16_t(p[1]));
}

inline uint32_t GetBe32(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) |
         uint32_t(p[3]);
}

inline uint64_t GetBe64(const uint8_t* p) {
  return (uint64_t(GetBe32(p)) << 32) | uint64_t(GetBe32(p + 4));
}

inline void Pad4(std::vector<uint8_t>* o) {
  while (o->size() % 4) o->push_back(0);
}

}  // namespace wire
}  // namespace wns

#endif  // WNS_WIRE_H_
