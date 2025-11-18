#include "sync_merge.h"
#include "config.h"

// ============================================================================
// 3-WAY MERGE IMPLEMENTATION
// ============================================================================

int SyncMerge::findWinner(uint64_t api_ts, uint64_t local_ts, uint64_t self_ts) {
    // IMPORTANT: timestamp = 0 has different meanings:
    // - For API (server): 0 = priority flag (server forcing this value)
    // - For Local/Self: 0 = uninitialized (never been set, should lose)
    //
    // Priority flag (timestamp=0) is ONLY used when SENDING to server.
    // On device-side merge, only API timestamp=0 means priority.

    // If API has priority flag (timestamp=0), API always wins
    if (api_ts == 0) {
        DEBUG_PRINTLN("[Merge] API has priority flag (ts=0), API wins");
        return 1;
    }

    // For Local and Self, timestamp=0 means "not set" - exclude from comparison
    bool local_is_set = (local_ts > 0);
    bool self_is_set = (self_ts > 0);

    // If only one source is set, it wins by default
    if (!local_is_set && !self_is_set) {
        // Neither local nor self set - API wins
        return 1;
    }
    if (!local_is_set && self_is_set) {
        // Only self is set - compare API vs Self
        return (self_ts > api_ts) ? 3 : 1;
    }
    if (local_is_set && !self_is_set) {
        // Only local is set - compare API vs Local
        return (local_ts > api_ts) ? 2 : 1;
    }

    // All three are set - use Last-Write-Wins (newest timestamp)
    uint64_t newest = api_ts;
    int winner = 1;

    if (local_ts > newest) {
        newest = local_ts;
        winner = 2;
    }

    if (self_ts > newest) {
        newest = self_ts;
        winner = 3;
    }

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
