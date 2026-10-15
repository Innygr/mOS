# What mOS takes from Mission Link

Source: [Innygr/Mission-Link](https://github.com/Innygr/Mission-Link) at the commit in `lib/missionlink/UPSTREAM_COMMIT`.

## Reused as-is (vendored in `lib/missionlink/`)

| Mission Link piece | Used for in mOS |
|---|---|
| `packet_codec` (protocol v1, 32-byte header) | Wire format. A watch frame must be accepted by any Mission Link node. |
| `mesh_router` | Run in the **Client** preset: delivers frames addressed to the watch, deduplicates, never relays (matches the decision that stationary nodes are the backbone). |
| `missionlink_radio` (SX1262 / RadioLib adapter) | Receive-only radio path with the verified Wio-SX1262 pin map; `transmit()` is blocked by `ML_RF_ENABLED=0`. |
| `missionlink_config.h` | Protocol limits and the RF gate. |
| Node id scheme (`"ML"` + 6 MAC bytes) | Same identity as Mission Link firmware. |
| Bounded JSON-Lines console idea | `mos_console`: `status`, `inbox`, `send`, `broadcast`, `time`, plus `sim`/`tap`/`swipe` for bring-up without a radio or touch controller. |
| Safety conventions (AGENTS.md) | Adapted in `AGENTS.md`. |

## How Mission Link sits in mOS

Mission Link is an **app**, not the OS. mOS owns the hardware and shared services; apps plug in through `mos::App` (`lib/mos_core/src/mos_app.h`).

| Layer | Where | Responsibility |
|---|---|---|
| OS services | `lib/mos_core` | Shell (watch face, launcher, app lifecycle, toasts, screen sleep), touch gestures, clock, layout helpers, serial console |
| System radio | `src/main.cpp` + `lib/missionlink` | Owns the SX1262 and the Client-preset router; hands locally-delivered packets to apps (`Shell::dispatchPacket`) and gives apps a transmit sink (`SystemContext::radio`) |
| Apps | `lib/mos_app_*` | Mission Link (inbox, reader, canned replies, acks/retries), Stopwatch, Settings (persisted via `IStore`), System (radio/device status) |

An app implements `name()`, `render()`, optionally `onGesture()` (return true if handled; unhandled swipe-right closes the app, unhandled long-press goes home), `tick()` (runs in the background even when closed), `onPacket()`, `badge()` (shown on the watch face) and `onCommand()` (adds serial console commands). `tests: test_mos_core` exercises the OS with a dummy app and no Mission Link code.

## Mission Link app internals (`lib/mos_app_missionlink/`)

- `mos_message` – payload `[kind][body]`: text (≤120 B) or a 1-byte canned phrase id (cheap on air). Non-printable bytes are sanitised before display.
- `mos_inbox` – 16-message RAM ring.
- `mos_messaging` – inbox/outbox, acks for direct messages (Ack carries the acked message id, only accepted from the node we sent to), retry with doubling timeout (3 attempts, provisional), and a **radio-blocked state that does not burn retry attempts**.
- `missionlink_app` – the screens (inbox, message reader, reply picker) and console commands `inbox`, `outbox`, `send`, `broadcast`, `sim`.

## OS pieces (`lib/mos_core/`)

`mos_shell` (face + launcher + app host), `mos_gesture` (tap / long-press / four-way swipe from raw touch samples), `mos_clock` (manually set; time sync is an open question), `mos_layout`, `mos_console` (`help`, `status`, `apps`, `open`, `home`, `time`, `tap`, `swipe`, then each app's commands).

## Not taken from Mission Link

BLE GATT interface, the web/PWA and Apple clients, the host-services registry and Node presets other than Client. The architecture decision is that the watch talks to the mesh over its own LoRa radio, not BLE.

## Known limitations / follow-ups

- **No transmission, no encryption.** Payloads are plaintext; the outbox holds messages (`Outbox N (held)` on the status screen) and nothing reaches the radio. Needs Mission Link's cipher design first.
- **Retry vs. receiver dedup:** a retry reuses the message id, and the Mission Link router drops repeats within 60 s (`DUPLICATE_TTL_MS`) before the app sees them, so a lost *ack* is not re-sent. Needs an upstream change (e.g. re-ack duplicates) or a retry timeout above the TTL.
- **Display/touch are assumptions:** SSD1306 128x64 I2C OLED on D4/D5 and a CST816S-style touch controller (INT D1, RST D2) are placeholders. The UI only needs the `Canvas` interface, so swapping panels means replacing `OledCanvas` in `src/main.cpp` (touch coordinates are assumed to equal display pixels).
- Power: the radio stays in continuous receive and the loop polls every 20–50 ms. Scheduled receive windows and light/deep sleep are future work.
- No contacts/addressing: replies go to the sender of the message being viewed; `broadcast`/`send <id>` exist only on the serial console.
