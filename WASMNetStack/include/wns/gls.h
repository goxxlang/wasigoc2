#ifndef WNS_GLS_H_
#define WNS_GLS_H_

#include "wns/chunk.h"

#include <cstdint>
#include <span>
#include <vector>

namespace wns {

enum class LabelOp : uint8_t { kPush = 0, kSwap = 1, kPop = 2 };

struct IlmEntry {
  uint32_t in_label = 0;
  LabelOp op = LabelOp::kSwap;
  uint32_t out_label = 0;
  uint32_t next_node = 0;
  uint16_t out_stream = 0;
};

enum class GlsResult : uint8_t { kDrop = 0, kForward = 1, kDeliver = 2 };

class Gls {
 public:
  void SetIlm(IlmEntry e);
  const IlmEntry* Lookup(uint32_t in_label) const;

  LabelData Push(uint32_t label, std::span<const uint8_t> user, uint8_t ttl = 64);
  GlsResult Process(LabelData* chunk, std::vector<uint8_t>* delivered, IlmEntry* used);

 private:
  std::vector<IlmEntry> ilm_;
};

}  // namespace wns

#endif  // WNS_GLS_H_
