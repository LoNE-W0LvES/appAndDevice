#include "sync_merge.h"
#include "config.h"

// ============================================================================
// 3-WAY MERGE IMPLEMENTATION
// ============================================================================

int SyncMerge::findWinner(uint64_t api_ts, uint64_t local_ts, uint64_t self_ts) {
    // IMPORTANT: timestamp = 0 means "uninitialized" or "not synced" for ALL sources
    // - API ts=0: No data from server yet (not synced)
    // - Local ts=0: Not set by app yet (uninitialized)
    // - Self ts=0: Not set by device yet (uninitialized)
    //
    // Last-Write-Wins: Highest non-zero timestamp wins
    // Sources with ts=0 are excluded from comparison (uninitialized)

    // Check which sources have valid timestamps (> 0)
    bool api_is_set = (api_ts > 0);
    bool local_is_set = (local_ts > 0);
    bool self_is_set = (self_ts > 0);

    // If only one source is set, it wins by default
    if (api_is_set && !local_is_set && !self_is_set) {
        DEBUG_PRINTLN("[Merge] Only API set, API wins");
        return 1;
    }
    if (!api_is_set && local_is_set && !self_is_set) {
        DEBUG_PRINTLN("[Merge] Only Local set, Local wins");
        return 2;
    }
    if (!api_is_set && !local_is_set && self_is_set) {
        DEBUG_PRINTLN("[Merge] Only Self set, Self wins");
        return 3;
    }

    // If none are set, API wins by default (fallback)
    if (!api_is_set && !local_is_set && !self_is_set) {
        DEBUG_PRINTLN("[Merge] None set, API wins by default");
        return 1;
    }

    // Two or more sources are set - use Last-Write-Wins (newest timestamp)
    uint64_t newest = 0;
    int winner = 1;

    if (api_is_set && api_ts > newest) {
        newest = api_ts;
        winner = 1;
    }

    if (local_is_set && local_ts > newest) {
        newest = local_ts;
        winner = 2;
    }

    if (self_is_set && self_ts > newest) {
        newest = self_ts;
        winner = 3;
    }

    DEBUG_PRINTF("[Merge] Last-Write-Wins: winner=%d, newest_ts=%llu\n", winner, newest);
    return winner;
}

bool SyncMerge::mergeBool(SyncBool& sync) {
    int winner = findWinner(sync.api_lastModified, sync.local_lastModified, sync.lastModified);

    bool oldValue = sync.value;

    switch (winner) {
        case 1:  // API wins - update Self to match API
            sync.value = sync.api_value;
            sync.lastModified = sync.api_lastModified;
            // Don't update api_value or local_value - they represent what was last received
            DEBUG_PRINTLN("[Merge] API won for boolean");
            break;

        case 2:  // Local wins - update Self to match Local
            sync.value = sync.local_value;
            sync.lastModified = sync.local_lastModified;
            // Don't update api_value or local_value - they get updated on next fetch
            DEBUG_PRINTLN("[Merge] Local won for boolean");
            break;

        case 3:  // Self wins - no update needed
            // Don't update api_value or local_value
            DEBUG_PRINTLN("[Merge] Self won for boolean");
            break;

        default:
            DEBUG_PRINTLN("[Merge] No change for boolean");
            return false;
    }

    // Return true if value actually changed
    return (sync.value != oldValue);
}

bool SyncMerge::mergeFloat(SyncFloat& sync) {
    int winner = findWinner(sync.api_lastModified, sync.local_lastModified, sync.lastModified);

    float oldValue = sync.value;

    switch (winner) {
        case 1:  // API wins - update Self to match API
            sync.value = sync.api_value;
            sync.lastModified = sync.api_lastModified;
            // Don't update api_value or local_value - they represent what was last received
            DEBUG_PRINTLN("[Merge] API won for float");
            break;

        case 2:  // Local wins - update Self to match Local
            sync.value = sync.local_value;
            sync.lastModified = sync.local_lastModified;
            // Don't update api_value or local_value - they get updated on next fetch
            DEBUG_PRINTLN("[Merge] Local won for float");
            break;

        case 3:  // Self wins - no update needed
            // Don't update api_value or local_value
            DEBUG_PRINTLN("[Merge] Self won for float");
            break;

        default:
            return false;
    }

    // Return true if value actually changed
    return (abs(sync.value - oldValue) > 0.001f);
}

bool SyncMerge::mergeString(SyncString& sync) {
    int winner = findWinner(sync.api_lastModified, sync.local_lastModified, sync.lastModified);

    String oldValue = sync.value;

    switch (winner) {
        case 1:  // API wins - update Self to match API
            sync.value = sync.api_value;
            sync.lastModified = sync.api_lastModified;
            // Don't update api_value or local_value - they represent what was last received
            DEBUG_PRINTLN("[Merge] API won for string");
            break;

        case 2:  // Local wins - update Self to match Local
            sync.value = sync.local_value;
            sync.lastModified = sync.local_lastModified;
            // Don't update api_value or local_value - they get updated on next fetch
            DEBUG_PRINTLN("[Merge] Local won for string");
            break;

        case 3:  // Self wins - no update needed
            // Don't update api_value or local_value
            DEBUG_PRINTLN("[Merge] Self won for string");
            break;

        default:
            return false;
    }

    // Return true if value actually changed
    return (sync.value != oldValue);
}
