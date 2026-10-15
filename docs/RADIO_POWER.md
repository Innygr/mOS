# Radio receive power (design note)

Status: **model and tests only. Nothing here is enabled in firmware, and none of it is measured on hardware.**

## Problem
The SX1262 sits in continuous receive. That is the largest battery cost on the watch (see ARCHITECTURE.md). The obvious fix is to let the radio sleep between short listen windows (RadioLib exposes this as receive duty cycling, e.g. `startReceiveDutyCycleAuto`).

## Why it is a network decision, not a watch setting
A receiver that wakes every *T* only hears a frame whose **preamble lasts longer than T**. So every transmitting node (backbone nodes, other watches) must use a preamble of at least `ceil(sleep / symbol time) + listen symbols`. Mission Link currently uses the RadioLib default of 8 symbols. If a watch duty-cycled against 8-symbol senders it would silently miss most messages.

## Numbers (SF9, 125 kHz, 4 listen symbols; symbol = 4.096 ms)
Produced by `mos::planDutyCycle` (`lib/mos_core/src/mos_radio_power.h`, covered by `test_lora_symbol_time_and_duty_plan`):

| Receiver sleeps | Sender preamble | Extra airtime per frame | Receiver awake |
|---:|---:|---:|---:|
| 100 ms | 29 symbols | +86 ms | 14.1% |
| 250 ms | 66 symbols | +238 ms | 6.2% |
| 500 ms | 127 symbols | +487 ms | 3.2% |
| 1000 ms | 249 symbols | +987 ms | 1.6% |
| 2000 ms | 493 symbols | +1987 ms | 0.8% |

Longer preambles also occupy the channel longer, so every transmission costs more shared airtime, which matters with scarce LoRa airtime and duty-cycle rules. The sleep period is also worst-case delivery latency for incoming messages.

## What is needed before enabling it
1. **Maintainer decision** on a network-wide preamble length / sleep period (a Mission Link protocol parameter; it should live in the Mission-Link repo and be vendored here).
2. **Upstream radio adapter change:** `missionlink_radio.cpp` needs a configurable preamble length for TX and a duty-cycled receive mode. It is vendored here, so the change belongs in Mission-Link first.
3. **Backbone nodes keep continuous receive** (mains/solar powered); only watches duty-cycle.
4. **Measure on real hardware:** actual sleep current, wake overhead, and missed-packet rate. The model ignores oscillator start-up and detection margins.
5. Confirm Canadian (ISED) rules for the longer transmissions before any TX is enabled (`ML_RF_ENABLED` stays 0).

## Other power levers (not started)
Lower display timeout (Settings already has 10 s), lighter loop polling, ESP32 light sleep between touch/radio interrupts, and dimming the OLED.
