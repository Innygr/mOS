#pragma once
// RAM-backed IStore for host tests and headless runs.
#include <map>
#include <string>
#include "mos_app.h"

namespace mos {
class MemStore : public IStore {
 public:
  bool getU32(const char* key, uint32_t& out) override {
    auto it = m_.find(key);
    if (it == m_.end()) return false;
    out = it->second;
    return true;
  }
  bool putU32(const char* key, uint32_t value) override { m_[key] = value; return true; }

 private:
  std::map<std::string, uint32_t> m_;
};
}  // namespace mos
