# Server Configuration Sync Logic

## Overview
This document specifies the server-side logic for handling device configuration updates from ESP32-S3 AquaFlow devices.

## Three Update Rules

### Rule 1: Priority Flag (lastModified == 0)
**Condition:** `incoming.timestamp == 0 AND incoming.value != current.value`

**Action:** ALWAYS accept the incoming value

**Reason:**
- Device sends `lastModified=0` to indicate local changes with priority
- These are user changes made via physical buttons while offline
- Must override any server/cloud dashboard changes
- Only update if values actually differ

**Example:**
```json
// Device sends (offline button change)
{
  "upperThreshold": {
    "key": "upperThreshold",
    "value": 85,
    "timestamp": 0  ← Priority flag
  }
}

// Server has
{
  "upperThreshold": {
    "value": 90,
    "timestamp": 1737123456789
  }
}

// Result: Accept device value (85), assign new server timestamp
{
  "upperThreshold": {
    "value": 85,
    "timestamp": 1737123999999  ← Server assigns new timestamp
  }
}
```

### Rule 2: Value Unchanged (Optimization)
**Condition:** `incoming.value == current.value`

**Action:** SKIP update entirely (no database write)

**Reason:**
- Values are identical - no actual change
- Timestamp difference is irrelevant
- Prevents unnecessary database writes
- Saves database operations and reduces wear

**Example:**
```json
// Device sends
{
  "upperThreshold": {
    "value": 90,
    "timestamp": 1737123456000
  }
}

// Server has
{
  "upperThreshold": {
    "value": 90,  ← Same value
    "timestamp": 1737123456789  ← Different timestamp
  }
}

// Result: NO UPDATE - values are identical
```

### Rule 3: Last-Write-Wins (Normal Case)
**Condition:** `incoming.value != current.value AND incoming.timestamp > 0 AND current.timestamp > 0`

**Action:** Use the value with the NEWER timestamp

**Reason:**
- Traditional conflict resolution
- Both device and server have valid timestamps
- Most recent change wins

**Example:**
```json
// Device sends (older change)
{
  "upperThreshold": {
    "value": 85,
    "timestamp": 1737123456000  ← Older
  }
}

// Server has (newer change from dashboard)
{
  "upperThreshold": {
    "value": 90,
    "timestamp": 1737123999000  ← Newer
  }
}

// Result: Keep server value (90) - it's newer
```

## Server-Side Implementation (Pseudocode)

```python
def update_device_config(device_id, incoming_config):
    """
    Handle device configuration update with 3-rule logic

    Args:
        device_id: Unique device identifier
        incoming_config: Configuration from device with nested structure

    Returns:
        Updated configuration with server-assigned timestamps
    """
    # Get current config from database
    current_config = db.get_config(device_id)
    updated_fields = []
    server_timestamp = get_current_unix_timestamp_ms()

    # Process each configuration field
    for field_name in CONFIG_FIELDS:
        incoming_field = incoming_config.get(field_name, {})
        current_field = current_config.get(field_name, {})

        incoming_value = incoming_field.get('value')
        incoming_timestamp = incoming_field.get('timestamp', 0)
        current_value = current_field.get('value')
        current_timestamp = current_field.get('timestamp', 0)

        # RULE 2: Value unchanged → Skip update
        if incoming_value == current_value:
            logger.debug(f"{field_name}: Values identical, skipping update")
            continue

        # RULE 1: Priority flag → Always accept (if value differs)
        if incoming_timestamp == 0:
            logger.info(f"{field_name}: Priority flag detected, accepting device value")
            current_config[field_name]['value'] = incoming_value
            current_config[field_name]['timestamp'] = server_timestamp
            updated_fields.append(field_name)
            continue

        # RULE 3: Last-Write-Wins → Use newer timestamp
        if incoming_timestamp > current_timestamp:
            logger.info(f"{field_name}: Incoming timestamp newer, accepting update")
            current_config[field_name]['value'] = incoming_value
            current_config[field_name]['timestamp'] = incoming_timestamp
            updated_fields.append(field_name)
        else:
            logger.info(f"{field_name}: Current timestamp newer, rejecting update")

    # Save to database only if changes were made
    if updated_fields:
        db.save_config(device_id, current_config)
        logger.info(f"Config updated for {device_id}: {updated_fields}")
    else:
        logger.info(f"No config changes for {device_id}")

    # Return updated config with success flag
    return {
        "success": True,
        "timestamp": server_timestamp,
        "config": current_config,
        "updated_fields": updated_fields
    }
```

## API Endpoint Specification

### POST /api/device/config

**Request Format:**
```json
{
  "deviceId": "wt001-DEV-02-690e6a9d092433c0acfb9178",
  "upperThreshold": {
    "key": "upperThreshold",
    "value": 85.0,
    "timestamp": 0  // 0 = priority flag, or Unix timestamp in milliseconds
  },
  "lowerThreshold": {
    "key": "lowerThreshold",
    "value": 20.0,
    "timestamp": 0
  },
  "tankHeight": {
    "key": "tankHeight",
    "value": 100.0,
    "timestamp": 0
  },
  "tankWidth": {
    "key": "tankWidth",
    "value": 50.0,
    "timestamp": 0
  },
  "tankShape": {
    "key": "tankShape",
    "value": "CYLINDRICAL",
    "timestamp": 0
  },
  "UsedTotal": {
    "key": "UsedTotal",
    "value": 1250.5,
    "timestamp": 0
  },
  "maxInflow": {
    "key": "maxInflow",
    "value": 10.0,
    "timestamp": 0
  },
  "force_update": {
    "key": "force_update",
    "value": false,
    "timestamp": 0
  },
  "ip_address": {
    "key": "ip_address",
    "value": "192.168.1.100",
    "timestamp": 0
  }
}
```

**Response Format:**
```json
{
  "success": true,
  "timestamp": 1737123999999,
  "config": {
    "upperThreshold": {
      "key": "upperThreshold",
      "value": 85.0,
      "timestamp": 1737123999999
    },
    "lowerThreshold": {
      "key": "lowerThreshold",
      "value": 20.0,
      "timestamp": 1737123999999
    }
    // ... other fields
  },
  "updated_fields": ["upperThreshold", "lowerThreshold"]
}
```

## Configuration Fields

All fields use the nested structure `{key, value, timestamp}`:

1. **upperThreshold** (float) - Water level % to turn pump OFF
2. **lowerThreshold** (float) - Water level % to turn pump ON
3. **tankHeight** (float) - Tank height in cm
4. **tankWidth** (float) - Tank width/diameter in cm
5. **tankShape** (string) - "CYLINDRICAL" or "RECTANGULAR"
6. **UsedTotal** (float) - Total water consumed in liters (note capital U)
7. **maxInflow** (float) - Maximum inflow rate in L/min
8. **force_update** (bool) - Force config update flag (snake_case)
9. **ip_address** (string) - Device local IP address (snake_case)

## Device Behavior

### When device_config_sync_status = false (Local Changes)
```
1. Device has local changes (button press, etc.)
2. Device reconnects to server
3. Compares local values with server values
4. If values differ:
   - Sends config with timestamp = 0 (priority flag)
   - Server applies Rule 1: accepts device values
5. If values identical:
   - Skips sync entirely
   - Marks as synced (device_config_sync_status = true)
```

### When device_config_sync_status = true (No Local Changes)
```
1. Device has no local changes
2. Device reconnects to server
3. Fetches config FROM server
4. Compares incoming values with current values
5. If values differ:
   - Applies server values
6. If values identical:
   - Skips update, just updates timestamp reference
```

## Testing Scenarios

### Test 1: Offline Device Change (Priority Flag)
```
1. Device offline, user presses button: upperThreshold = 80
2. device_config_sync_status = false
3. Meanwhile, admin changes dashboard: upperThreshold = 95
4. Device reconnects, sends with timestamp = 0
5. Server accepts 80 (Rule 1: priority flag)
✅ Expected: Server has upperThreshold = 80
```

### Test 2: Identical Values, Different Timestamps
```
1. Device: upperThreshold = 90, timestamp = 1000
2. Server: upperThreshold = 90, timestamp = 2000
3. Device sends update
4. Server applies Rule 2: values identical, skip
✅ Expected: No database write
```

### Test 3: Concurrent Changes (Last-Write-Wins)
```
1. Device online, changes upperThreshold = 85 at T=1000
2. Dashboard changes upperThreshold = 90 at T=2000
3. Device syncs with timestamp = 1000
4. Server applies Rule 3: dashboard timestamp newer
✅ Expected: Server keeps upperThreshold = 90
```

### Test 4: Device Change While Online
```
1. Device online with device_config_sync_status = true
2. User presses button: upperThreshold = 75
3. device_config_sync_status = false immediately
4. Device sends config with timestamp = 0 within seconds
5. Server applies Rule 1: accepts 75
✅ Expected: Near-instant sync, server has 75
```

## Database Schema Suggestion

```sql
CREATE TABLE device_config (
    device_id VARCHAR(255) PRIMARY KEY,
    upper_threshold FLOAT,
    upper_threshold_timestamp BIGINT,  -- Unix ms
    lower_threshold FLOAT,
    lower_threshold_timestamp BIGINT,
    tank_height FLOAT,
    tank_height_timestamp BIGINT,
    -- ... other fields with timestamps
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Index for timestamp comparisons
CREATE INDEX idx_config_timestamps ON device_config (
    upper_threshold_timestamp,
    lower_threshold_timestamp
    -- ... other timestamp fields
);
```

## Security Considerations

1. **Authentication:** All requests must include valid JWT token
2. **Rate Limiting:** Limit config updates to prevent abuse
3. **Validation:** Validate all incoming values (ranges, types)
4. **Audit Log:** Track all config changes with device/user/timestamp
5. **Priority Flag Abuse:** Monitor for excessive use of timestamp=0

## Performance Optimizations

1. **Value Comparison First:** Check Rule 2 before database query
2. **Batch Updates:** Update multiple fields in single transaction
3. **Caching:** Cache current config to reduce database reads
4. **Async Processing:** Handle config updates asynchronously if possible

## Error Handling

```json
// Invalid value range
{
  "success": false,
  "error": "INVALID_VALUE",
  "message": "upperThreshold must be between 0 and 100",
  "field": "upperThreshold"
}

// Authentication failure
{
  "success": false,
  "error": "UNAUTHORIZED",
  "message": "Invalid or expired token"
}

// Database error
{
  "success": false,
  "error": "DATABASE_ERROR",
  "message": "Failed to save configuration"
}
```

## Monitoring Metrics

Track these metrics for system health:

- `config_updates_total` - Total config updates
- `config_updates_priority` - Updates with timestamp=0
- `config_updates_skipped` - Skipped due to identical values
- `config_updates_lww` - Last-Write-Wins updates
- `config_update_latency` - Time to process update
- `config_conflicts` - Cases where server timestamp was newer

## Notes

- All timestamps are Unix epoch in **milliseconds** (not seconds)
- Priority flag (timestamp=0) only applies when values differ
- Server must assign new timestamp when accepting priority updates
- Device expects updated config with timestamps in response
- Value comparison is type-sensitive (float 90.0 != int 90)
