#pragma once
// RAM-backed IStore for host tests and headless runs.
#include <map>
#include <string.h>
#include <string>
#include <vector>
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
  size_t getBlob(const char* key, uint8_t* buf, size_t cap) override {
    auto it = b_.find(key);
    if (it == b_.end() || it->second.size() > cap) return 0;
    memcpy(buf, it->second.data(), it->second.size());
    return it->second.size();
  }
  bool putBlob(const char* key, const uint8_t* data, size_t len) override {
    b_[key].assign(data, data + len);
    ++blobWrites;
    return true;
  }
  unsigned blobWrites = 0;  // lets tests check flash-wear behaviour

 private:
  std::map<std::string, uint32_t> m_;
  std::map<std::string, std::vector<uint8_t>> b_;
};
}  // namespace mos
