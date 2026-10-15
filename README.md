# mOS

A smartwatch-style OS for the Seeed XIAO ESP32-S3 with a touchscreen OLED and a Wio-SX1262 LoRa radio. It runs [Mission Link](https://github.com/Innygr/Mission-Link) natively: the radio is a system service and the touch UI sits on top.

See [ARCHITECTURE.md](ARCHITECTURE.md) for decisions and open questions, and [docs/MISSION_LINK_PORT.md](docs/MISSION_LINK_PORT.md) for what was adapted from Mission Link.

## Status

Early scaffold. **Not tested on hardware.** RF transmission is disabled, messages are not encrypted, and the display/touch parts are placeholders until the exact panel is confirmed.

## What works today

- Receive-only Mission Link radio path (shared with Mission-Link firmware) feeding an inbox.
- Watch UI: face, inbox, message reader, canned replies, status. Swipe left/up from the face; swipe right to go back; tap a row to open; tap `[ Reply ]`.
- Direct-message acks and retry logic, held in an outbox while TX is blocked.
- Serial console for bring-up: `help`, `status`, `inbox`, `sim <text>`, `time <epoch> [offset-min]`, `tap <x> <y>`, `swipe <dir>`, `send <id> <text>`, `broadcast <text>`.

## Build and test

```sh
pio test -e native          # protocol, messaging, gestures, UI, console (host)
pio run -e xiao_esp32s3     # compile firmware
```

Pins and defines live in `platformio.ini`. Run `sim hello` over serial to see a message arrive without a radio.
