# mOS roadmap

## M0 — Foundation
- [x] ESP-IDF scaffold targeting XIAO ESP32S3.
- [x] Mission Link packet codec separated from radio driver.
- [x] Draft frame specification and compatibility statement.
- [ ] Build and run on actual board.
- [x] Add codec tests, host-test workflow, and a fixed golden test vector.

## M1 — Mission Link compatibility first
- [ ] Review and freeze a versioned packet format.
- [x] Add host-testable transport orchestration for queued frame preparation, TX result reporting, receive dispatch, and ACK frame generation.\n- [ ] Add hardware radio adapter and radio status/error handling.
- [ ] Integrate exact SX1262 board and verified pin mapping.
- [x] Add text-message creation and nonzero message-ID generation.
- [x] Add bounded volatile outgoing queue and explicit delivery-state model.
- [x] Add bounded receive inbox with sender-scoped duplicate suppression.
- [x] Add host-testable receive frame validation, destination filtering, and duplicate-aware ACK recommendation.\n- [ ] Integrate receive helper with radio transport and implement ACK handling.
- [x] Define draft empty-payload ACK wire format and validate peer/message correlation.\n- [ ] Integrate ACK helpers with hardware radio transport, retries/backoff, and end-to-end delivery path.
- [ ] Demonstrate text delivery through a stationary node.

## M2 — Watch basics
- [ ] Confirm display, touch/buttons, battery, and charging hardware.
- [ ] Add display/input abstraction and home screen.
- [x] Add a driver-independent configurable screen idle-timeout policy (30-second default) with host tests.
- [ ] Add Clock, Settings, and Messages apps.
- [ ] Show node ID, radio state, signal metrics, and delivery status.

## M3 — Reliability and security
- [ ] Define routing and store-and-forward.
- [x] Add host-testable ACK retry/backoff scheduler.\n- [ ] Add retry metadata persistence, message expiry, and end-to-end reliability testing.
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
