# mOS contributor instructions

Read [ARCHITECTURE.md](ARCHITECTURE.md) and [docs/MISSION_LINK_PORT.md](docs/MISSION_LINK_PORT.md) first.

- The human maintainer decides. Keep changes focused; prefer a branch + PR; never merge without approval.
- **RF safety (inherited from Mission-Link):** `ML_RF_ENABLED` stays `0`. Do not enable transmission on assumptions about pinout, antenna, RF switch, frequency, or Canadian (ISED) rules. The messaging service additionally refuses to hand plaintext to the radio until an encryption layer exists (`MessagingService::Config::encryptionReady`).
- Do not claim range, encryption, delivery guarantees, or hardware compatibility without evidence. Report source review, native tests, ESP32 compile, and real-hardware tests separately.
- `lib/missionlink/` is vendored from Mission-Link (see `lib/missionlink/UPSTREAM_COMMIT`). Change protocol behaviour upstream first, then re-sync; do not fork the wire format silently.
- Display and touch hardware are not chosen yet; keep OS and app UI code on the `Canvas` / gesture abstractions so it stays testable on the host. Apps must reach the radio, clock and notifications only through `SystemContext`, never hardware directly.
- Never commit secrets or private device identifiers.
