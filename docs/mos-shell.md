# mOS shell and apps

Host-tested C components (`tests/run_host_tests.sh`). Nothing here is wired to a display or touch driver yet because those parts are still open hardware decisions.

## Shell (`components/mos_shell`)
- **Canvas** (`mos_canvas_t`): a 1-bit drawing surface of function pointers; a display driver fills it in. Tests use a recording canvas.
- **Gestures** (`mos_gesture_*`): tap, long press and four-way swipes from raw down/move/up samples.
- **Clock** (`mos_clock_*`): set explicitly (no RTC/time sync yet), extrapolated from a monotonic ms clock, UTC offset by hand, no DST.
- **Layout** (`mos_layout.h`): shared header/row sizes and word wrapping.
- **Shell** (`mos_shell_*`): watch face (time, app badges, radio indicator), launcher, app host, toasts and screen sleep through the shared `mos_screen_timeout` policy (default 30 s, clamped 5 s to 5 min, see [mos-screen-power.md](mos-screen-power.md); the first touch after sleep only wakes). `mos_shell_poll_screen_off()` returns true once when the display driver should power down.

Navigation: face --swipe left--> launcher --tap--> app. An app handles the gestures it wants; an unhandled swipe right closes it and an unhandled long press goes home.

An app is a `mos_app_t` descriptor (name, optional badge/open/tick/gesture callbacks, required render). Apps tick in the background even when closed.

## Apps (`components/mos_apps`)
Per [mos-app-catalog.md](mos-app-catalog.md):
- **Messages:** inbox (newest first, unread marked), reader with scrolling, canned replies that queue a text via `ml_service_make_text`, and an outbox page that shows queued / sent / delivered / FAILED distinctly. Long press deletes a message or removes a finished outbox entry. It only views the inbox/outbox owned by the Mission Link service.
- **Settings:** read-only device facts (node, radio state, TX on/off, signal, battery, firmware). No control is offered for hardware that is not confirmed.
- **App Store:** visible but inactive; it only explains why. No install/download controls exist.
- **Contacts:** not implemented yet.

## Known gaps
- **The inbox does not evict.** `ml_inbox_receive` rejects new messages when 8 are stored (`ML_RECEIVE_INBOX_FULL`), so an unattended watch stops receiving. The Messages app lets the wearer delete, but an eviction/expiry policy is needed.
- Nothing persists across reboot (inbox, outbox, retry state). The prototype's validated, debounced inbox format is a candidate to port.
- Replies are plain text; there is no compact canned-phrase encoding yet (payload encoding is still undefined in the protocol doc).
- No firmware glue: `main/main.c` does not create a shell, and no display, touch or radio driver exists on main.
- Not yet built under ESP-IDF in this environment (ESP-IDF is not installed here); the components are plain C and are compile-checked with the host compiler only.
