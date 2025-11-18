#include "sync_merge.h"
#include "config.h"

// ============================================================================
// 3-WAY MERGE IMPLEMENTATION
// ============================================================================

int SyncMerge::findWinner(uint64_t api_ts, uint64_t local_ts, uint64_t self_ts) {
    // Priority flag check: timestamp = 0 means "force accept"
    // Priority order: api (1) > local (2) > self (3) when multiple have priority

    if (api_ts == 0 && local_ts == 0 && self_ts == 0) {
        // All have priority flag - API wins by default
        return 1;
    }

    if (api_ts == 0) {
        // API has priority flag - API wins
        return 1;
    }

    if (local_ts == 0) {
        // Local has priority flag - Local wins
        return 2;
    }

    if (self_ts == 0) {
        // Self has priority flag - Self wins
        return 3;
    }

    // No priority flags - use Last-Write-Wins (newest timestamp)
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
