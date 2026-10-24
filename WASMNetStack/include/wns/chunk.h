#ifndef WNS_CHUNK_H_
#define WNS_CHUNK_H_

#include "wns/addr.h"
#include "wns/sctp.h"

#include <cstdint>
#include <span>
#include <vector>

namespace wns {

struct SctpHeader {
  uint16_t src_port = sctp::kPort;
  uint16_t dst_port = sctp::kPort;
  uint32_t vtag = 0;
  uint32_t checksum = 0;
};

struct InitChunk {
  bool ack = false;
  uint32_t initiate_tag = 0;
  uint32_t a_rwnd = 65535;
  uint16_t outbound = 16;
  uint16_t inbound = 16;
  uint32_t initial_tsn = 1;
  std::vector<Ipv4> ipv4;
  std::vector<Ipv6> ipv6;
  std::vector<uint8_t> cookie;
};

struct DataChunk {
  uint8_t flags = sctp::kDataB | sctp::kDataE;
  uint32_t tsn = 0;
  uint16_t sid = 0;
  uint16_t ssn = 0;
  uint32_t ppid = 0;
  std::vector<uint8_t> user;
};

struct Label {
  uint32_t value = 0;
  uint8_t tc = 0;
  bool bottom = true;
  uint8_t ttl = 64;
};

struct LabelData {
  uint8_t flags = sctp::kDataB | sctp::kDataE;
  uint16_t ssn = 0;
  std::vector<Label> stack;
  std::vector<uint8_t> user;
};

uint32_t PackLabel(const Label& l);
Label UnpackLabel(uint32_t w);

std::vector<uint8_t> EncodeInit(const InitChunk& i);
bool DecodeInit(std::span<const uint8_t> wire, InitChunk* out);

std::vector<uint8_t> EncodeData(const DataChunk& d);
bool DecodeData(std::span<const uint8_t> wire, DataChunk* out);

std::vector<uint8_t> EncodeLabelData(const LabelData& d);
bool DecodeLabelData(std::span<const uint8_t> wire, LabelData* out);

std::vector<uint8_t> EncodePacket(const SctpHeader& h, std::span<const uint8_t> chunk);
bool DecodePacket(std::span<const uint8_t> wire, SctpHeader* h, std::span<const uint8_t>* chunk);

uint8_t ChunkType(std::span<const uint8_t> chunk);

}  // namespace wns

#endif  // WNS_CHUNK_H_
