#pragma once
// System serial console (JSON Lines, same style as Mission Link's). Handles OS
// commands and offers every other command to the apps. Includes `sim`-free
// touch injection (`tap`, `swipe`) so the UI can be driven without hardware.
#include <string>
#include "mos_shell.h"

namespace mos {
std::string handleCommand(const std::string& line, Shell& shell, WatchClock& clock,
                          const DeviceInfo& dev, uint32_t nowMs);
}  // namespace mos
