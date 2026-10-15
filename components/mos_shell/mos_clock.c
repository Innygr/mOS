#include "mos_clock.h"

#include <string.h>

void mos_clock_init(mos_clock_t *clock)
{
    if (clock != NULL) {
        memset(clock, 0, sizeof(*clock));
    }
}

void mos_clock_set_epoch(mos_clock_t *clock, uint32_t epoch_s, uint64_t now_ms)
{
    if (clock == NULL) {
        return;
    }
    clock->base_epoch_s = epoch_s;
    clock->base_ms = now_ms;
    clock->valid = true;
}

void mos_clock_set_utc_offset_min(mos_clock_t *clock, int16_t minutes)
{
    if (clock != NULL) {
        clock->utc_offset_min = minutes;
    }
}

bool mos_clock_time_of_day(const mos_clock_t *clock, uint64_t now_ms,
                           uint8_t *hour, uint8_t *minute, uint8_t *second)
{
    int64_t local, sod;

    if (clock == NULL || !clock->valid || hour == NULL || minute == NULL ||
        second == NULL) {
        return false;
    }
    local = (int64_t)clock->base_epoch_s +
            (int64_t)((now_ms - clock->base_ms) / 1000u) +
            (int64_t)clock->utc_offset_min * 60;
    sod = local % 86400;
    if (sod < 0) {
        sod += 86400;
    }
    *hour = (uint8_t)(sod / 3600);
    *minute = (uint8_t)((sod / 60) % 60);
    *second = (uint8_t)(sod % 60);
    return true;
}
