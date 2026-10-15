#include "mos_screen_timeout.h"

static uint32_t clamp_timeout(uint32_t timeout_ms)
{
    if (timeout_ms < MOS_SCREEN_TIMEOUT_MIN_MS) return MOS_SCREEN_TIMEOUT_MIN_MS;
    if (timeout_ms > MOS_SCREEN_TIMEOUT_MAX_MS) return MOS_SCREEN_TIMEOUT_MAX_MS;
    return timeout_ms;
}

void mos_screen_timeout_init(mos_screen_timeout_t *state, uint32_t now_ms, uint32_t timeout_ms)
{
    if (state == 0) return;
    state->timeout_ms = clamp_timeout(timeout_ms);
    state->last_activity_ms = now_ms;
    state->screen_on = true;
    state->initialized = true;
}

void mos_screen_timeout_activity(mos_screen_timeout_t *state, uint32_t now_ms)
{
    if (state == 0 || !state->initialized) return;
    state->last_activity_ms = now_ms;
    state->screen_on = true;
}

bool mos_screen_timeout_poll(mos_screen_timeout_t *state, uint32_t now_ms)
{
    uint32_t elapsed;
    if (state == 0 || !state->initialized || !state->screen_on) return false;
    /* Unsigned subtraction tolerates clock wrap for these short intervals. */
    elapsed = now_ms - state->last_activity_ms;
    if (elapsed < state->timeout_ms) return false;
    state->screen_on = false;
    return true;
}

void mos_screen_timeout_set(mos_screen_timeout_t *state, uint32_t timeout_ms)
{
    if (state == 0 || !state->initialized) return;
    state->timeout_ms = clamp_timeout(timeout_ms);
}
