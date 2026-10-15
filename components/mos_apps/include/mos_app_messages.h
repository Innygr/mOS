#ifndef MOS_APP_MESSAGES_H
#define MOS_APP_MESSAGES_H

#include "mission_link_inbox.h"
#include "mission_link_queue.h"
#include "mission_link_service.h"
#include "mos_shell.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Messages app (see docs/mos-app-catalog.md). It is a view over the Mission
 * Link inbox/outbox that the system service owns; it never touches the radio.
 *
 *   Inbox   : swipe up/down scroll, tap row -> Message, swipe left -> Outbox
 *   Message : swipe up/down scroll, tap bottom bar -> Reply,
 *             long press -> delete, swipe right -> Inbox
 *   Reply   : tap a canned phrase to queue it to the sender, swipe right -> Message
 *   Outbox  : queued / sent / delivered / FAILED shown distinctly; long press a
 *             finished entry to remove it, swipe right -> Inbox
 * An unhandled swipe right (on the Inbox page) closes the app.
 */
typedef enum {
    MOS_MSG_PAGE_INBOX = 0,
    MOS_MSG_PAGE_MESSAGE,
    MOS_MSG_PAGE_REPLY,
    MOS_MSG_PAGE_OUTBOX
} mos_messages_page_t;

#define MOS_MSG_REPLY_TTL 4u

typedef struct {
    mos_shell_t *shell;
    ml_inbox_t *inbox;
    ml_outbox_t *outbox;
    ml_service_t *service;
    mos_messages_page_t page;
    size_t scroll;
    bool have_selection;
    uint32_t sel_source;
    uint32_t sel_id;
    int rows;           /* layout cached from the last render */
    int canvas_height;
    int cols;
} mos_messages_app_t;

/* Fills `app_out` with a descriptor to register via mos_shell_add_app(). */
void mos_messages_app_init(mos_messages_app_t *app, mos_app_t *app_out,
                           mos_shell_t *shell, ml_inbox_t *inbox,
                           ml_outbox_t *outbox, ml_service_t *service);

#ifdef __cplusplus
}
#endif

#endif
