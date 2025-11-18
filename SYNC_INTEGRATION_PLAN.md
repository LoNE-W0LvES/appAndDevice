# 3-Way Sync Integration Plan

## Overview
Implement 3-way synchronization for ESP32 device and 2-way for Flutter app.

## Architecture

### Device (ESP32) - 3 Sources:
- **API** - Data from cloud server
- **Local** - Data from app via webserver
- **Self** - Device's own stored value

### App (Flutter) - 2 Sources:
- Uses **either** API **or** Local (based on mode)
- Self - App's own stored value

## Sync Flow Example

```
1. RECEIVE DATA FROM 3 SOURCES:
   API:   value=20, timestamp=10
   Local: value=11, timestamp=20  ← Winner (newest)
   Self:  value=24, timestamp=14

2. MERGE (Local wins):
   After merge, ALL become:
   API:   value=11, timestamp=20
   Local: value=11, timestamp=20
   Self:  value=11, timestamp=20

3. BUILD JSON (use Self.value):
   {
     "pumpSwitch": {
       "key": "pumpSwitch",
       "label": "Pump Switch",
       "type": "boolean",
       "value": 11,  ← Use Self.value
       "lastModified": 0  ← Priority flag
     }
   }

4. SEND TO SERVER with priority flag

5. SERVER RESPONSE (assigns new timestamp):
   API:   value=11, timestamp=25
   Local: value=11, timestamp=20
   Self:  value=11, timestamp=22
   → Values identical, no further sync needed
```

## Integration Steps

### Phase 1: Device Integration

#### 1.1 Create Global Handlers (main.cpp)
```cpp
// Add to global scope
ControlDataHandler controlHandler;
ConfigDataHandler configHandler;
TelemetryDataHandler telemetryHandler;

// Initialize in initializeSystem()
controlHandler.begin();
configHandler.begin();
telemetryHandler.begin();
```

#### 1.2 Update Webserver (webserver.cpp)
- **GET /control**: Return `controlHandler.value` (not api_value or local_value)
- **POST /control**: Call `controlHandler.updateFromLocal()`, then `merge()`
- **GET /config**: Return `configHandler.value`
- **POST /config**: Call `configHandler.updateFromLocal()`, then `merge()`
- **GET /telemetry**: Return `telemetryHandler` values

#### 1.3 Update API Client (api_client.cpp)
- **fetchControl()**: Call `controlHandler.updateFromAPI()`, then `merge()`
- **uploadControl()**: Use `controlHandler.value` with priority flag
- **fetchConfig()**: Call `configHandler.updateFromAPI()`, then `merge()`
- **uploadConfig()**: Use `configHandler.value` with priority flag

#### 1.4 Update Main Loop (main.cpp)
- Read sensor → Update `telemetryHandler.update()`
- Apply pump control from `controlHandler.getPumpSwitch()`
- Apply config from `configHandler.getXXX()`
- Periodic: Merge control, Merge config

### Phase 2: Flutter App Integration

#### 2.1 Create Global Handlers (Provider/Service)
```dart
class SyncProvider extends ChangeNotifier {
  final controlHandler = ControlDataHandler();
  final configHandler = ConfigDataHandler();
  final telemetryHandler = TelemetryDataHandler();

  SyncMode currentMode = SyncMode.local; // or SyncMode.api
}
```

#### 2.2 Update Services
- **getDeviceLiveData()**:
  - Fetch from API or Local
  - Call `controlHandler.updateFromAPI()` or `updateFromLocal()`
  - Call `controlHandler.merge(currentMode)`
  - Return merged values

- **updateControl()**:
  - Call `controlHandler.updateSelf()`
  - Build JSON using `controlHandler.value` with priority flag
  - Send to API or Local
  - Fetch response and merge

#### 2.3 Update UI
- Read from handlers: `controlHandler.getPumpSwitch()`
- Write to handlers: `controlHandler.setPumpSwitchPriority(true)`
- Trigger merge after updates

### Phase 3: Priority Flag Usage

#### When to Use Priority Flag (timestamp=0):
1. **App button press** - User manually toggles pump
2. **Device boot** - Send initial config to server
3. **Manual override** - Force value regardless of timestamp

#### When NOT to Use:
1. **Normal periodic sync** - Use actual timestamps
2. **Reading from server** - Server provides timestamps

## Data Flow Diagrams

### Device Receives from App (Local):
```
App → POST /control →
  updateFromLocal(value=true, ts=20) →
  merge() →
  Apply to pump →
  Upload to server with priority flag
```

### Device Receives from Server (API):
```
Server → fetchControl() →
  updateFromAPI(value=false, ts=25) →
  merge() →
  Apply to pump
```

### Device 3-Way Merge:
```
API (ts=10) ←┐
              ├→ merge() → Winner → Update all 3
Local (ts=20) ← Winner
              ↓
Self (ts=14) ←┘
```

## Testing Checklist

- [ ] Device: Receive from server, merge, apply
- [ ] Device: Receive from app, merge, apply
- [ ] Device: Both sources conflict, newest wins
- [ ] Device: Priority flag always wins
- [ ] Device: Send to server with priority
- [ ] App: API mode, merge self vs api
- [ ] App: Local mode, merge self vs local
- [ ] App: Switch modes, values preserved
- [ ] App: Send to device with priority
- [ ] App: Send to server with priority

## File Changes Required

### Device (ESP32):
- ✅ `sync_types.h` - New
- ✅ `sync_merge.h/cpp` - New
- ✅ `handle_control_data.h/cpp` - New
- ✅ `handle_config_data.h/cpp` - New
- ✅ `handle_telemetry_data.h/cpp` - New
- ⏳ `main.cpp` - Modify (add handlers, merge calls)
- ⏳ `webserver.cpp` - Modify (use handlers)
- ⏳ `api_client.cpp` - Modify (use handlers)

### App (Flutter):
- ✅ `sync_types.dart` - New
- ✅ `sync_merge.dart` - New
- ✅ `handle_control_data.dart` - New
- ⏳ `handle_config_data.dart` - New
- ⏳ `handle_telemetry_data.dart` - New
- ⏳ `offline_device_service.dart` - Modify (use handlers)
- ⏳ `device_service.dart` - Modify (use handlers)
- ⏳ `sync_provider.dart` - New (manage handlers)

## Next Steps

1. ✅ Create this integration plan
2. ⏳ Complete Flutter handlers (config + telemetry)
3. ⏳ Integrate into device code
4. ⏳ Integrate into app code
5. ⏳ Test end-to-end
