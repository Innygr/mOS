#ifndef MOS_APP_STORE_H
#define MOS_APP_STORE_H

#include "mos_shell.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * App Store placeholder (catalog: visible but inactive). It only explains why
 * it is unavailable: no download, install or "success" UI exists until an app
 * API, package format, signature verification, permissions and rollback do.
 */
void mos_app_store_placeholder_init(mos_app_t *app_out);

#ifdef __cplusplus
}
#endif

#endif
