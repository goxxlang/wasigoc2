#include "wns/gls.h"

namespace wns {

void Gls::SetIlm(IlmEntry e) {
  for (auto& x : ilm_) {
    if (x.in_label == e.in_label) {
      x = e;
      return;
    }
  }
  ilm_.push_back(e);
}

const IlmEntry* Gls::Lookup(uint32_t in_label) const {
  for (const auto& x : ilm_) {
    if (x.in_label == in_label) return &x;
  }
  return nullptr;
}

LabelData Gls::Push(uint32_t label, std::span<const uint8_t> user, uint8_t ttl) {
  LabelData d;
  Label l;
  l.value = label;
  l.bottom = true;
  l.ttl = ttl;
  d.stack.push_back(l);
  d.user.assign(user.begin(), user.end());
  return d;
}

GlsResult Gls::Process(LabelData* chunk, std::vector<uint8_t>* delivered, IlmEntry* used) {
  if (!chunk || chunk->stack.empty()) return GlsResult::kDrop;
  if (chunk->stack.size() > 16) return GlsResult::kDrop;
  auto* e = Lookup(chunk->stack.front().value);
  if (!e) return GlsResult::kDrop;
  if (used) *used = *e;
  if (chunk->stack.front().ttl == 0) return GlsResult::kDrop;

  if (e->op == LabelOp::kSwap) {
    chunk->stack.front().value = e->out_label;
    if (chunk->stack.front().ttl > 0) chunk->stack.front().ttl--;
    if (chunk->stack.front().ttl == 0) return GlsResult::kDrop;
    return GlsResult::kForward;
  }
  if (e->op == LabelOp::kPop) {
    bool bottom = chunk->stack.front().bottom;
    chunk->stack.erase(chunk->stack.begin());
    if (bottom || chunk->stack.empty()) {
      if (delivered) *delivered = chunk->user;
      return GlsResult::kDeliver;
    }
    return GlsResult::kForward;
  }
  return GlsResult::kDrop;
}

}  // namespace wns
