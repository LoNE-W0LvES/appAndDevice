#ifndef SYNC_MERGE_H
#define SYNC_MERGE_H

#include "sync_types.h"

// ============================================================================
// 3-WAY MERGE LOGIC (Last-Write-Wins)
// ============================================================================
// Priority flag: timestamp = 0 means "always accept this value"
// Otherwise: compare timestamps and accept the newest value

class SyncMerge {
public:
    // Merge boolean value (3-way for device)
    // Returns true if value changed
    static bool mergeBool(SyncBool& sync);

    // Merge float value (3-way for device)
    // Returns true if value changed
    static bool mergeFloat(SyncFloat& sync);

    // Merge string value (3-way for device)
    // Returns true if value changed
    static bool mergeString(SyncString& sync);

private:
    // Find which source has the winning value
    // Returns: 0=no change, 1=api wins, 2=local wins, 3=self wins
    static int findWinner(uint64_t api_ts, uint64_t local_ts, uint64_t self_ts);
};

#endif // SYNC_MERGE_H
