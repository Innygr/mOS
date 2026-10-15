# mOS starting app catalog

This is the initial launcher catalog. These are product requirements, not yet implemented UI apps.

## Messages

**Status:** core app, planned.

- Show conversations grouped by saved contact or raw Mission Link ID.
- Compose and receive text over Mission Link.
- Offer Bluetooth as an alternate transport when a compatible peer/service is available; do not imply Bluetooth automatically bridges to the LoRa mesh.
- Show queued, sent, delivered, and failed as distinct states.
- Keep offline/queued messages visible and make capacity limits understandable.
- Never label a message delivered solely because it was handed to a radio.

## Settings

**Status:** core app, planned.

- Device preferences such as time/date, display, sound, and haptics where supported by selected hardware.
- Connectivity status and supported connection configuration.
- Device/firmware information and diagnostics.
- Expose only settings that the detected hardware and firmware actually support.
- Do not present unverified radio pins or unsupported radio controls as working.

## Contacts

**Status:** core app, planned.

- Store a display name and the person's Mission Link human-readable ID.
- Keep that ID distinct from the internal numeric radio node address.
- Allow opening a message conversation from a contact.
- Define a persistence strategy before promising contacts survive reset.
- Avoid assuming the phone-number-shaped `(777)-XXX-XXXX` ID is a real telephone number or can be called through the cellular network.

## App Store

**Status:** visible placeholder; inactive.

- May appear in the launcher from the beginning.
- Opening it should clearly explain that the store is not available yet.
- Installation, downloads, updates, and package execution stay disabled until an app lifecycle/API, package format, signature verification, permissions, and rollback/recovery strategy exist.
- No pretend download buttons or false success states.

## Launcher and shared rules

- The four initial entries are Messages, Settings, Contacts, and App Store.
- The inactive App Store should be visually distinguished but not hidden.
- Use clear focus/selection states suitable for the final display and input hardware; exact layout waits until the screen and controls are selected.
- Core Mission Link messaging should not require a paired phone.
- Bluetooth is an optional connection path, not a requirement for the watch's basic operation.
