#ifndef MOS_APP_SETTINGS_H
#define MOS_APP_SETTINGS_H

#include "mos_shell.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Settings (catalog: core app). Read-only for now: it shows only facts the
 * firmware actually knows, and exposes no control for hardware that has not
 * been confirmed (display, battery, haptics, radio pins are still open).
 */
typedef struct {
    const mos_device_info_t *device;
} mos_settings_app_t;

void mos_settings_app_init(mos_settings_app_t *app, mos_app_t *app_out,
                           const mos_device_info_t *device);

#ifdef __cplusplus
}
#endif

#endif
