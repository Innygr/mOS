# mOS

A smartwatch-style OS for the Seeed XIAO ESP32-S3 with a touchscreen OLED and a Wio-SX1262 LoRa radio. The OS provides the shell (watch face, launcher), touch input, and the LoRa radio as a system service; apps plug in on top. [Mission Link](https://github.com/Innygr/Mission-Link) is the first app.

See [ARCHITECTURE.md](ARCHITECTURE.md) for decisions and open questions, and [docs/MISSION_LINK_PORT.md](docs/MISSION_LINK_PORT.md) for what was adapted from Mission Link.

## Status

Early scaffold. **Not tested on hardware.** RF transmission is disabled, messages are not encrypted, and the display/touch parts are placeholders until the exact panel is confirmed.

## What works today

- OS shell: watch face with app badges, launcher, app lifecycle, notifications, screen sleep. Swipe left on the face for the launcher; swipe right to go back; long-press to go home.
- Apps: **Mission Link** (inbox that survives reboots, message reader, canned replies, acks/retries), **Stopwatch**, **Settings** (UTC offset and screen timeout, saved to flash) and **System** (radio/device status). Adding an app means implementing `mos::App`.
- Receive-only LoRa radio as a system service (same adapter as Mission-Link firmware), delivering packets to apps.
- Serial console for bring-up: `help`, `status`, `apps`, `open <app>`, `home`, `time <epoch> [offset-min]`, `tap <x> <y>`, `swipe <dir>`, plus app commands (Mission Link: `inbox`, `outbox`, `sim <text>`, `send <id> <text>`, `broadcast <text>`).

## Build and test

```sh
pio test -e native          # OS shell/gestures/console and the Mission Link app (host)
pio run -e xiao_esp32s3     # compile firmware
```

Pins and defines live in `platformio.ini`. Run `sim hello` over serial to see a message arrive without a radio.
