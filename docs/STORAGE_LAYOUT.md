# mOS storage layout

## Storage policy

mOS Portable must boot and run its built-in apps without a TF/microSD card. Keep the bootloader, partition table, firmware, mOS core, built-in Mission Link app, Settings, System app, and essential configuration on internal ESP32-S3 flash. The current NVS-backed `IStore` remains the small persistent key/value store; do not silently move it to removable media.

Use the round display board's TF card (up to 32 GB, according to the hardware specification supplied for this project) for optional user files, exports/backups, larger diagnostic logs, and future optional app assets. Do not make boot or core messaging depend on it.

## Service boundary

`lib/mos_core/src/mos_storage.h` defines a hardware-independent `StorageService` and `IRemovableVolume` contract. A board-specific adapter is responsible for identifying the actual display board, confirming the TF slot pinout and bus sharing, selecting a supported filesystem, and implementing the adapter. The OS core must not guess GPIO pins or include Arduino SD libraries.

The service distinguishes unchecked, unavailable, ready, read-only, and I/O-error states. A missing card or I/O failure must not crash the shell. Writes are refused unless the volume is mounted and writable. A failed operation moves the service to I/O-error state until the adapter probes again. No automatic formatting is allowed.

## Boot sequence

1. Start firmware and core services from internal flash.
2. Start built-in apps even if removable storage is absent.
3. Probe optional storage through the board adapter.
4. Publish status and free-space information in the System app/Settings.
5. On absence or failure, remain operational and expose reduced-storage status.

## Safety and durability

- Keep frequently updated settings and essential state on internal flash using the existing bounded, versioned persistence paths.
- Avoid repeated whole-file rewrites; batch optional logs and exports.
- Never store credentials or private keys as plaintext on removable media.
- Treat the card as removable and corruptible; check every operation result.
- Do not format an unknown filesystem automatically.
- mOS Static may instantiate `StorageService` without an adapter.

## Hardware integration acceptance criteria

- [ ] Confirm exact round-display SKU and inspect its schematic before assigning pins.
- [ ] Watch boots with no card and all built-in apps remain usable.
- [ ] Test ready, read-only, absent, full-volume and I/O-error behaviour with a fake adapter.
- [ ] Test mount, read/write, removal and recovery on the real board.
- [ ] Confirm card access does not conflict with the LoRa radio or display bus.
