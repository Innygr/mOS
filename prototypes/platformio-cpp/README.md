# PlatformIO / Arduino prototype (reference only)

An earlier C++ build of the watch firmware: shell, Mission Link app, Stopwatch, Settings, persisted inbox, and a LoRa duty-cycle model, on top of the Mission-Link repo's firmware code (vendored in `lib/missionlink`, see `UPSTREAM_COMMIT`).

`main` is now ESP-IDF + C, so this is **not the build**. It is kept to port from and for its 42 host tests:

```sh
cd prototypes/platformio-cpp && pio test -e native
```

It uses the Mission-Link firmware wire format, which differs from `main`'s (see ../../docs/protocol-comparison.md). `ci.yml.disabled` is its old workflow, deliberately not active.
