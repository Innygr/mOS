#ifndef MOS_SCREEN_TIMEOUT_H
#define MOS_SCREEN_TIMEOUT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOS_SCREEN_TIMEOUT_DEFAULT_MS (30000u)
#define MOS_SCREEN_TIMEOUT_MIN_MS     (5000u)
#define MOS_SCREEN_TIMEOUT_MAX_MS     (300000u)

typedef struct {
    uint32_t timeout_ms;
    uint32_t last_activity_ms;
    bool screen_on;
    bool initialized;
} mos_screen_timeout_t;

void mos_screen_timeout_init(mos_screen_timeout_t *state, uint32_t now_ms, uint32_t timeout_ms);
void mos_screen_timeout_activity(mos_screen_timeout_t *state, uint32_t now_ms);
bool mos_screen_timeout_poll(mos_screen_timeout_t *state, uint32_t now_ms);
void mos_screen_timeout_set(mos_screen_timeout_t *state, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
#endif
