#ifndef MOS_CLOCK_H
#define MOS_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Wall clock for the watch face. There is no RTC or network time yet (time
 * sync is an open question), so time is set explicitly and extrapolated from
 * a monotonic millisecond clock. Daylight saving is not handled.
 */
typedef struct {
    bool valid;
    uint32_t base_epoch_s;
    uint64_t base_ms;
    int16_t utc_offset_min;
} mos_clock_t;

void mos_clock_init(mos_clock_t *clock);
void mos_clock_set_epoch(mos_clock_t *clock, uint32_t epoch_s, uint64_t now_ms);
void mos_clock_set_utc_offset_min(mos_clock_t *clock, int16_t minutes);
/* False if the clock has never been set. */
bool mos_clock_time_of_day(const mos_clock_t *clock, uint64_t now_ms,
                           uint8_t *hour, uint8_t *minute, uint8_t *second);

#ifdef __cplusplus
}
#endif

#endif
