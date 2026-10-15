# mOS roadmap

## M0 — Foundation
- [x] ESP-IDF scaffold targeting XIAO ESP32S3.
- [x] Mission Link packet codec separated from radio driver.
- [x] Draft frame specification and compatibility statement.
- [ ] Build and run on actual board.
- [ ] Add codec tests and fixed test vectors.

## M1 — Mission Link compatibility first
- [ ] Review and freeze a versioned packet format.
- [ ] Add transport API for sending/receiving frames and radio status.
- [ ] Integrate exact SX1262 board and verified pin mapping.
- [ ] Add node identity and message-ID generation.
- [ ] Implement receive validation, duplicate suppression, and local inbox.
- [ ] Demonstrate text delivery through a stationary node.

## M2 — Watch basics
- [ ] Confirm display, touch/buttons, battery, and charging hardware.
- [ ] Add display/input abstraction and home screen.
- [ ] Add Clock, Settings, and Messages apps.
- [ ] Show node ID, radio state, signal metrics, and delivery status.

## M3 — Reliability and security
- [ ] Define routing and store-and-forward.
- [ ] Add ACKs, retries/backoff, expiry, and duplicate suppression.
- [ ] Design authenticated pairing and end-to-end encryption before private messaging.
- [ ] Add power-aware receive windows.
- [ ] Test multi-node delivery, weak links, restarts, and expiry.

## M4 — Platform features
- [ ] OTA updates with integrity verification and rollback.
- [ ] App lifecycle/API and permission model.
- [ ] Signed app packages and store after APIs stabilize.
- [ ] Keep companion phone app optional for core operation.

## Open hardware decisions
- Exact XIAO variant and SX1262 carrier/module revision.
- Display model, resolution, interface, touch controller, and pins.
- Battery capacity, charging/protection circuit, and enclosure dimensions.
- Antenna type and placement.
- Canadian radio-compliance review for selected band, module, antenna, and settings.
