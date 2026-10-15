#pragma once
// Built-in System app: radio and device status. Read-only.
#include "mos_app.h"

namespace mos {
class SystemApp : public App {
 public:
  const char* name() const override { return "System"; }
  const char* shortName() const override { return "SYS"; }
  void render(Canvas& canvas, uint32_t nowMs) override;
};
}  // namespace mos
