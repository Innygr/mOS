// mOS firmware entry point for the XIAO ESP32-S3 watch.
// Hardware glue only. The OS (lib/mos_core), the apps (lib/mos_app_*) and the
// vendored Mission Link protocol (lib/missionlink) are all host-tested.
// This file owns the radio (a system service) and feeds the Shell.
#include <Arduino.h>
#include <Wire.h>
#include <esp_mac.h>
#include <esp_random.h>
#include <string>
#include "missionlink_config.h"
#include "missionlink_radio.h"
#include "mesh_router.h"
#include "mos_console.h"
#include "mos_shell.h"
#include "missionlink_app.h"
#include "system_app.h"

#if MOS_DISPLAY_SSD1306
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#endif

static constexpr size_t MAX_COMMAND_BYTES = 256;

// ---- display -----------------------------------------------------------------
#if MOS_DISPLAY_SSD1306
class OledCanvas : public mos::Canvas {
 public:
  OledCanvas() : d_(MOS_DISPLAY_W, MOS_DISPLAY_H, &Wire, -1) {}
  bool begin() {
    if (!d_.begin(SSD1306_SWITCHCAPVCC, MOS_DISPLAY_ADDR)) return false;
    d_.setTextWrap(false);
    d_.clearDisplay();
    d_.display();
    return true;
  }
  void power(bool on) { d_.ssd1306_command(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF); }
  int width() const override { return MOS_DISPLAY_W; }
  int height() const override { return MOS_DISPLAY_H; }
  void clear() override { d_.clearDisplay(); }
  void drawText(int x, int y, const char* t, int scale, bool inverted) override {
    d_.setTextSize(scale);
    d_.setTextColor(inverted ? SSD1306_BLACK : SSD1306_WHITE);
    d_.setCursor(x, y);
    d_.print(t);
  }
  void fillRect(int x, int y, int w, int h, bool on) override {
    d_.fillRect(x, y, w, h, on ? SSD1306_WHITE : SSD1306_BLACK);
  }
  void drawRect(int x, int y, int w, int h) override { d_.drawRect(x, y, w, h, SSD1306_WHITE); }
  void present() override { d_.display(); }

 private:
  Adafruit_SSD1306 d_;
};
#else
class OledCanvas : public mos::Canvas {  // headless: serial console only
 public:
  bool begin() { return false; }
  void power(bool) {}
  int width() const override { return 128; }
  int height() const override { return 64; }
  void clear() override {}
  void drawText(int, int, const char*, int, bool) override {}
  void fillRect(int, int, int, int, bool) override {}
  void drawRect(int, int, int, int) override {}
  void present() override {}
};
#endif

// ---- touch (CST816S-style: gesture, fingers, xH, xL, yH, yL at reg 0x01) ----------
#if MOS_TOUCH_CST816S
static constexpr uint8_t TOUCH_ADDR = 0x15;
static bool readTouch(int16_t& x, int16_t& y) {
  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(0x01);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(static_cast<int>(TOUCH_ADDR), 6) != 6) return false;
  Wire.read();  // built-in gesture id: we classify gestures ourselves
  const uint8_t fingers = Wire.read();
  const uint8_t xh = Wire.read(), xl = Wire.read(), yh = Wire.read(), yl = Wire.read();
  if (fingers == 0) return false;
  x = static_cast<int16_t>(((xh & 0x0f) << 8) | xl);
  y = static_cast<int16_t>(((yh & 0x0f) << 8) | yl);
  return true;
}
static void touchReset() {
  pinMode(MOS_TOUCH_RST, OUTPUT);
  digitalWrite(MOS_TOUCH_RST, LOW);
  delay(10);
  digitalWrite(MOS_TOUCH_RST, HIGH);
  delay(50);
}
#else
static bool readTouch(int16_t&, int16_t&) { return false; }
static void touchReset() {}
#endif

// ---- radio sink ----------------------------------------------------------------
class RadioSink : public mos::IPacketSink {
 public:
  explicit RadioSink(ml::MissionLinkRadio& r) : radio_(r) {}
  mos::TxResult transmit(const ml::Packet& p) override {
    if (radio_.transmit(p)) return mos::TxResult::Sent;
    return radio_.status().state == ml::RadioState::TransmitBlocked ? mos::TxResult::Blocked
                                                                    : mos::TxResult::Error;
  }

 private:
  ml::MissionLinkRadio& radio_;
};

static const char* radioStateName(ml::RadioState s) {
  switch (s) {
    case ml::RadioState::Disabled: return "disabled";
    case ml::RadioState::PinmapUnverified: return "pinmap?";
    case ml::RadioState::Ready: return "ready";
    case ml::RadioState::ReceiveError: return "rx error";
    case ml::RadioState::DecodeError: return "decode err";
    case ml::RadioState::TransmitBlocked: return "tx blocked";
    case ml::RadioState::TransmitError: return "tx error";
  }
  return "unknown";
}

// ---- globals ---------------------------------------------------------------------
static uint8_t localNodeId[8] = {0};
static OledCanvas canvas;
static ml::MissionLinkRadio radioLink;
static RadioSink sink(radioLink);
static ml::MeshRouter router(ml::NodePreset::Client);  // a watch never relays
static mos::WatchClock watchClock;
static mos::Shell* shell = nullptr;
static mos::SystemApp systemApp;
static mos::MissionLinkApp* missionLink = nullptr;
static mos::DeviceInfo device;
static mos::GestureRecognizer recognizer;
static bool displayOn = true;
static std::string commandBuffer;
static bool discardUntilNewline = false;

static void refreshDevice() {
  const ml::RadioStatus rs = radioLink.status();
  device.radioState = radioStateName(rs.state);
  device.rfEnabled = ML_RF_ENABLED;
  device.receiving = rs.receiving;
  device.rssi = rs.lastRssi;
  device.snr = rs.lastSnr;
  device.preset = ml::presetName(router.preset());
}

static void pollTouch(uint32_t now) {
  int16_t x, y;
  if (readTouch(x, y)) {
    if (!recognizer.active()) recognizer.touchDown(x, y, now);
    else recognizer.touchMove(x, y);
  } else if (recognizer.active()) {
    shell->onGesture(recognizer.touchUp(now), now);
  }
}

static void pollSerial() {
  while (Serial.available()) {
    const char ch = static_cast<char>(Serial.read());
    if (ch == '\n' || ch == '\r') {
      if (discardUntilNewline) {
        discardUntilNewline = false;
        commandBuffer.clear();
        Serial.println("{\"type\":\"error\",\"code\":\"command_too_long\"}");
      } else if (!commandBuffer.empty()) {
        const std::string reply = mos::handleCommand(commandBuffer, *shell, watchClock, device, millis());
        if (!reply.empty()) Serial.println(reply.c_str());
        commandBuffer.clear();
      }
    } else if (!discardUntilNewline) {
      if (commandBuffer.size() < MAX_COMMAND_BYTES) commandBuffer += ch;
      else { commandBuffer.clear(); discardUntilNewline = true; }  // drop the whole line
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("{\"type\":\"boot\",\"firmware\":\"" MOS_VERSION "\",\"rfEnabled\":false}");

  uint8_t mac[6] = {0};
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
    localNodeId[0] = 'M'; localNodeId[1] = 'L';  // same id scheme as Mission-Link firmware
    for (size_t i = 0; i < 6; ++i) localNodeId[i + 2] = mac[i];
  }
  ml::hexId(localNodeId, 8).copy(device.nodeIdHex, 16);

  Wire.begin(MOS_I2C_SDA, MOS_I2C_SCL);
  if (!canvas.begin()) Serial.println("{\"type\":\"warn\",\"message\":\"display not found\"}");
  touchReset();

  shell = new mos::Shell(canvas, watchClock, device, sink);
  missionLink = new mos::MissionLinkApp(localNodeId, esp_random());
  shell->addApp(missionLink);
  shell->addApp(&systemApp);

  radioLink.beginReceive();  // RX only; TX is blocked by ML_RF_ENABLED=0
  refreshDevice();
  Serial.println("{\"type\":\"hint\",\"message\":\"Send help for commands. RF TX is deliberately disabled.\"}");
}

void loop() {
  const uint32_t now = millis();
  pollSerial();
  pollTouch(now);

  ml::Packet incoming, forwarded;
  if (radioLink.poll(incoming)) {
    const ml::MeshAction action = router.process(incoming, localNodeId, now, forwarded);
    if (action == ml::MeshAction::DeliverLocal || action == ml::MeshAction::DeliverAndForward)
      shell->dispatchPacket(incoming, radioLink.status().lastRssi, now);
  }
  shell->tick(now);

  refreshDevice();
  const bool wantOn = !shell->asleep(now);
  if (wantOn != displayOn) { canvas.power(wantOn); displayOn = wantOn; }
  if (displayOn) shell->render(now);
  delay(displayOn ? 20 : 50);  // light polling; deep/light sleep is future work
}
