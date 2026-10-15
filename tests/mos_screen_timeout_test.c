#include <assert.h>
#include <stdint.h>
#include "mos_screen_timeout.h"

int main(void)
{
    mos_screen_timeout_t state;
    mos_screen_timeout_init(&state, 100u, MOS_SCREEN_TIMEOUT_DEFAULT_MS);
    assert(state.screen_on && state.timeout_ms == 30000u);
    assert(!mos_screen_timeout_poll(&state, 30099u));
    assert(mos_screen_timeout_poll(&state, 30100u));
    assert(!state.screen_on && !mos_screen_timeout_poll(&state, 50000u));
    mos_screen_timeout_activity(&state, 50001u);
    assert(state.screen_on && !mos_screen_timeout_poll(&state, 80000u));
    assert(mos_screen_timeout_poll(&state, 80001u));
    mos_screen_timeout_set(&state, 1u);
    assert(state.timeout_ms == MOS_SCREEN_TIMEOUT_MIN_MS);
    mos_screen_timeout_set(&state, UINT32_MAX);
    assert(state.timeout_ms == MOS_SCREEN_TIMEOUT_MAX_MS);
    mos_screen_timeout_init(&state, UINT32_MAX - 10000u, 5000u);
    assert(!mos_screen_timeout_poll(&state, 3000u));
    assert(mos_screen_timeout_poll(&state, 6000u));
    mos_screen_timeout_init(&state, 0u, 5000u);
    mos_screen_timeout_activity(&state, 100u);
    assert(!mos_screen_timeout_poll(&state, 5099u));
    assert(mos_screen_timeout_poll(&state, 5100u));
    return 0;
}
