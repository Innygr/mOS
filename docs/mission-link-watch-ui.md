# Mission Link watch UI decisions

Status: implementation requirements; firmware integration is still pending.

## Boot splash

- Use the Mission Link flag supplied by the project owner: https://www.crwflags.com/fotw/images/c/ca-bc-mi.gif
- Show the flag as the startup splash on the round 240x240 display.
- Keep the flag asset in firmware flash (embedded asset), not on the removable TF/microSD card. The card may be absent at boot.
- Do not overlay "starting", Wi-Fi, module-detection, or connection status text on the splash.
- Start network/time synchronization in the background. Keep the splash visible until normal startup transitions to the watch UI; background networking must not replace it with a connection screen.
- Use the first frame if the supplied GIF is animated, preserving its aspect ratio against a black background.

## Shared on-screen keyboard

Implement a reusable OS-level text-input service, not a keyboard that only exists inside Notes. Apps request text input and receive the edited value through a common interface.

Initial integration target: Notes. Later callers may include Wi-Fi credentials, messages, and contact names.

### Interaction requirements

- Compact touch keyboard sized for a circular 240x240 display.
- QWERTY letter layout with shift/case switching.
- Separate number/symbol layout and a way to return to letters.
- Space, backspace, clear/editing, save/confirm, and cancel controls.
- Keep the current text visible and editable while the keyboard is open.
- Handle touch input in the keyboard before normal launcher/app gestures so key presses do not accidentally navigate away.
- Save Notes through the existing notes storage path and report missing-card/write errors without discarding the draft.
- Keep the keyboard UI and text-input state independent from the Notes UI so future apps can reuse them.

## Storage and hardware constraints

- The board has a TF/microSD slot with up to 32 GB supported in the current hardware plan.
- Core OS, radio/LoRa support, and boot assets belong in onboard flash; removable storage is optional data storage, not a boot dependency.
- Preserve the existing board-specific SD chip-select distinction when the LoRa module is installed. Do not assume the camera and LoRa module use the same SD wiring.

## Verification checklist

- [ ] Splash asset is embedded and visible before app UI initialization completes.
- [ ] Wi-Fi/time sync can run without replacing the splash with a connection page.
- [ ] Keyboard supports letters, case, numbers/symbols, space, backspace, confirm, and cancel.
- [ ] Notes can create and persist text using the shared keyboard.
- [ ] Keyboard touches do not trigger underlying app/launcher gestures.
- [ ] Native build and ESP32-S3 compile pass.
- [ ] Hardware boot, touch, SD persistence, and background networking are tested on-device.

Do not mark implementation or hardware verification complete until those checks have actually passed.
