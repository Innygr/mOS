# mOS — Mission Link Watch

**Mission Link Watch, powered by mOS.**

mOS is a smartwatch-style OS for the Seeed XIAO ESP32S3, designed with **native Mission Link support as a first-class system service**. Core watch and mesh functions should work without a phone; a companion app can be optional.

> **Status:** early development scaffold. Host-tested codec, message service, inbox/outbox, ACK helpers, transport orchestration, and exponential ACK retry scheduler exist. Nothing here is a deployed or field-tested mesh network yet.

## First priorities
1. Mission Link compatibility from day one: a dedicated service and transport-independent packet framing.
2. Reliable ESP-IDF firmware foundation for the XIAO ESP32S3.
3. Hardware abstraction for display, input, battery, and SX1262 LoRa radio.
4. Launcher with Messages, Settings, Contacts, and an inactive App Store entry.
5. Power-aware networking: compact packets, scheduled receive windows, persistent retry state, and store-and-forward.
6. Secure updates and eventually a signed app store.

## Hardware direction
- MCU: Seeed XIAO ESP32S3.
- Mesh radio: SX1262 module, such as Wio-SX1262. The XIAO ESP32S3 alone does **not** include LoRa.
- USB-C: power, flashing, serial debugging, and data.
- Display, battery, and pin assignments remain open until exact parts are chosen.

## Build scaffold
Install ESP-IDF 5.x, open its terminal, and run:

~~~sh
idf.py set-target esp32s3
idf.py build
~~~

Then connect over USB-C and use `idf.py flash monitor`. Current firmware only logs startup and a heartbeat; no screen or radio driver is wired up yet.

## Repository map
- `main/` — firmware startup.
- `components/mission_link/` — packet codec, message service, bounded inbox/outbox, receive pipeline, ACK helpers, transport orchestration, and host-testable retry scheduler.
- `components/mos_shell/` — watch face, launcher, app host, gestures, clock, layout (host-tested).
- `components/mos_apps/` — Messages, Settings, and the inactive App Store placeholder.
- `docs/mos-shell.md` — shell/app design and known gaps. `docs/protocol-comparison.md` — wire-format decision.
- `prototypes/platformio-cpp/` — earlier Arduino/PlatformIO build, reference only.
- `docs/mission-link-protocol.md` — draft wire format.
- `docs/mission-link-transport.md` — transport boundary, retry/backoff, and hardware integration notes.\n- `docs/roadmap.md` — milestones and open hardware decisions.
- `docs/mos-app-catalog.md` — starting apps and their intended behavior.

## Compatibility
Mission Link is a custom protocol, **not Meshtastic or MeshCore compatible**. The packet format is a draft and may change before an interoperable release.

Radio settings, antenna design, and transmit power must be checked against applicable Canadian ISED requirements before transmitting.

## License
MIT — see [LICENSE](LICENSE).
