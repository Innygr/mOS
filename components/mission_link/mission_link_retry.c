#include "mission_link_retry.h"

#include <string.h>

static bool find_outbox_entry(const ml_outbox_t *outbox,
                              uint32_t message_id,
                              ml_outbox_entry_t *entry_out)
{
    size_t i;

    if (outbox == NULL || outbox->count > ML_OUTBOX_CAPACITY) return false;
    for (i = 0; i < outbox->count; ++i) {
        if (outbox->entries[i].packet.message_id == message_id) {
            if (entry_out != NULL) *entry_out = outbox->entries[i];
            return true;
        }
    }
    return false;
}

static ml_retry_entry_t *find_retry(ml_retry_state_t *retry,
                                    uint32_t message_id)
{
    size_t i;
    for (i = 0; i < ML_RETRY_CAPACITY; ++i) {
        if (retry->entries[i].active &&
            retry->entries[i].message_id == message_id) {
            return &retry->entries[i];
        }
    }
    return NULL;
}

static uint64_t timeout_for_attempt(uint32_t base_timeout_ms,
                                    uint32_t attempts)
{
    uint64_t timeout = base_timeout_ms;
    uint32_t shifts = attempts > 0u ? attempts - 1u : 0u;

    while (shifts > 0u && timeout < UINT64_MAX / 2u) {
        timeout *= 2u;
        --shifts;
    }
    if (shifts > 0u) return UINT64_MAX;
    return timeout;
}

void ml_retry_init(ml_retry_state_t *retry)
{
    if (retry != NULL) memset(retry, 0, sizeof(*retry));
}

bool ml_retry_note_tx(ml_retry_state_t *retry,
                      const ml_outbox_t *outbox,
                      uint32_t message_id,
                      uint64_t now_ms)
{
    ml_outbox_entry_t outbox_entry;
    ml_retry_entry_t *entry;
    size_t i;

    if (retry == NULL || message_id == 0u ||
        !find_outbox_entry(outbox, message_id, &outbox_entry) ||
        outbox_entry.state != ML_DELIVERY_SENT) {
        return false;
    }

    entry = find_retry(retry, message_id);
    if (entry == NULL) {
        for (i = 0; i < ML_RETRY_CAPACITY; ++i) {
            if (!retry->entries[i].active) {
                entry = &retry->entries[i];
                memset(entry, 0, sizeof(*entry));
                entry->active = true;
                entry->message_id = message_id;
                break;
            }
        }
    }
    if (entry == NULL || entry->attempts == UINT32_MAX) return false;

    ++entry->attempts;
    entry->last_tx_ms = now_ms;
    return true;
}

static ml_retry_entry_t *get_or_create_retry(ml_retry_state_t *retry,
                                             uint32_t message_id)
{
    ml_retry_entry_t *entry = find_retry(retry, message_id);
    size_t i;

    if (entry != NULL) return entry;
    for (i = 0; i < ML_RETRY_CAPACITY; ++i) {
        if (!retry->entries[i].active) {
            entry = &retry->entries[i];
            memset(entry, 0, sizeof(*entry));
            entry->active = true;
            entry->message_id = message_id;
            return entry;
        }
    }
    return NULL;
}

bool ml_retry_note_tx_failed(ml_retry_state_t *retry,
                             ml_outbox_t *outbox,
                             uint32_t message_id,
                             uint32_t max_attempts)
{
    ml_outbox_entry_t outbox_entry;
    ml_retry_entry_t *entry;

    if (retry == NULL || outbox == NULL || message_id == 0u ||
        max_attempts == 0u ||
        !find_outbox_entry(outbox, message_id, &outbox_entry) ||
        outbox_entry.state != ML_DELIVERY_QUEUED) {
        return false;
    }

    entry = get_or_create_retry(retry, message_id);
    if (entry == NULL || entry->attempts == UINT32_MAX) return false;

    ++entry->attempts;
    if (entry->attempts >= max_attempts) {
        if (!ml_outbox_mark_failed(outbox, message_id)) return false;
        memset(entry, 0, sizeof(*entry));
    }
    return true;
}

size_t ml_retry_poll(ml_retry_state_t *retry,
                     ml_outbox_t *outbox,
                     uint64_t now_ms,
                     uint32_t base_timeout_ms,
                     uint32_t max_attempts)
{
    size_t i;
    size_t changed = 0u;

    if (retry == NULL || outbox == NULL || base_timeout_ms == 0u ||
        max_attempts == 0u || outbox->count > ML_OUTBOX_CAPACITY) {
        return 0u;
    }

    for (i = 0; i < ML_RETRY_CAPACITY; ++i) {
        ml_retry_entry_t *retry_entry = &retry->entries[i];
        ml_outbox_entry_t outbox_entry;
        uint64_t timeout;

        if (!retry_entry->active) continue;
        if (!find_outbox_entry(outbox, retry_entry->message_id, &outbox_entry) ||
            outbox_entry.state == ML_DELIVERY_DELIVERED ||
            outbox_entry.state == ML_DELIVERY_FAILED) {
            memset(retry_entry, 0, sizeof(*retry_entry));
            continue;
        }
        if (outbox_entry.state != ML_DELIVERY_SENT ||
            now_ms < retry_entry->last_tx_ms) {
            continue;
        }

        timeout = timeout_for_attempt(base_timeout_ms, retry_entry->attempts);
        if (now_ms - retry_entry->last_tx_ms < timeout) continue;

        if (retry_entry->attempts >= max_attempts) {
            if (ml_outbox_mark_failed(outbox, retry_entry->message_id)) {
                ++changed;
            }
            memset(retry_entry, 0, sizeof(*retry_entry));
        } else if (ml_outbox_requeue(outbox, retry_entry->message_id)) {
            ++changed;
        }
    }

    /* Adopt SENT messages nobody registered, so they cannot hang forever. */
    for (i = 0; i < outbox->count; ++i) {
        const ml_outbox_entry_t *oe = &outbox->entries[i];
        ml_retry_entry_t *adopted;

        if (oe->state != ML_DELIVERY_SENT ||
            find_retry(retry, oe->packet.message_id) != NULL) {
            continue;
        }
        adopted = get_or_create_retry(retry, oe->packet.message_id);
        if (adopted != NULL) {
            adopted->attempts = 1u;
            adopted->last_tx_ms = now_ms;
        }
    }
    return changed;
}
