# mOS screen idle timeout

mOS has a driver-independent screen-idle policy. The default timeout is 30 seconds; values are clamped to 5 seconds through 5 minutes. User activity resets the timer and marks the logical screen on. Polling after the timeout changes the logical state to off exactly once.

## Integration contract

- Use a monotonic millisecond clock (ESP-IDF can use esp_timer_get_time() / 1000).
- Call mos_screen_timeout_activity() for touch, button, or other intentional user input. It wakes the logical screen and restarts the timer.
- Poll mos_screen_timeout_poll() periodically. If it returns true, invoke the selected display driver's sleep/off operation.
- When activity wakes an off screen, compare the prior state or maintain a driver-state flag so the display driver's wake/on operation is issued once.
- The helper does not depend on a display library, GPIO map, or panel choice. The exact Seeed round-display driver and touch controller must be integrated before the physical screen can actually turn off.
- This policy does not power down the ESP32 or stop Mission Link networking; it defines screen idle behavior only.
- Timeout state is volatile and must be restored from settings if persistence is added later.

Unsigned clock subtraction tolerates 32-bit millisecond counter wrap for these short timeout intervals.

Host test: tests/mos_screen_timeout_test.c.
