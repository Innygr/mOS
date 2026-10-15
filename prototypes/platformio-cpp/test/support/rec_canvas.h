#pragma once
// Test canvas that records what was drawn.
#include <string>
#include <vector>
#include "mos_canvas.h"

struct RecCanvas : mos::Canvas {
  std::vector<std::string> texts;
  int presents = 0;
  int width() const override { return 128; }
  int height() const override { return 64; }
  void clear() override { texts.clear(); }
  void drawText(int, int, const char* t, int, bool) override { texts.push_back(t); }
  void fillRect(int, int, int, int, bool) override {}
  void drawRect(int, int, int, int) override {}
  void present() override { ++presents; }
  bool has(const char* needle) const {
    for (auto& t : texts) if (t.find(needle) != std::string::npos) return true;
    return false;
  }
};
