#include "system_app.h"
#include "mos_layout.h"
#include <stdio.h>

namespace mos {
void SystemApp::render(Canvas& c, uint32_t) {
  drawHeader(c, "System");
  const DeviceInfo* d = sys().device;
  if (!d) return;
  char buf[40];
  int y = headerH() + 1;
  auto line = [&](const char* s) { c.drawText(0, y, s); y += FONT_H; };
  snprintf(buf, sizeof(buf), "Radio %s", d->radioState); line(buf);
  snprintf(buf, sizeof(buf), "TX %s  %s", d->rfEnabled ? "ON" : "OFF", d->preset); line(buf);
  if (d->receiving) snprintf(buf, sizeof(buf), "RSSI %d SNR %d", d->rssi, d->snr);
  else snprintf(buf, sizeof(buf), "RSSI --");
  line(buf);
  if (d->batteryPercent >= 0) snprintf(buf, sizeof(buf), "Batt %d%%", d->batteryPercent);
  else snprintf(buf, sizeof(buf), "Batt --");
  line(buf);
  snprintf(buf, sizeof(buf), "%s", d->nodeIdHex); line(buf);
}
}  // namespace mos
