import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../models/device.dart';
import '../providers/device_provider.dart';
import '../providers/timestamp_provider.dart';
import '../screens/water_tank_control_screen.dart';
import '../screens/device_settings_screen.dart';
import '../screens/add_device_screen.dart';
import 'package:intl/intl.dart';

class DeviceSidebar extends StatefulWidget {
  const DeviceSidebar({Key? key}) : super(key: key);

  @override
  State<DeviceSidebar> createState() => _DeviceSidebarState();
}

class _DeviceSidebarState extends State<DeviceSidebar> {
  String _searchQuery = '';
  String _sortBy = 'name'; // name, status, lastSeen

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final isDark = theme.brightness == Brightness.dark;

    return Drawer(
      backgroundColor: isDark ? const Color(0xFF111827) : Colors.white,
      child: SafeArea(
        child: Column(
          children: [
            // Header
            Container(
              padding: const EdgeInsets.all(16),
              decoration: BoxDecoration(
                color: isDark ? const Color(0xFF1F2937) : const Color(0xFFF3F4F6),
                border: Border(
                  bottom: BorderSide(
                    color: isDark ? const Color(0xFF374151) : const Color(0xFFE5E7EB),
                  ),
                ),
              ),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(
                    children: [
                      Icon(
                        Icons.water_drop,
                        color: theme.primaryColor,
                        size: 32,
                      ),
                      const SizedBox(width: 12),
                      Expanded(
                        child: Text(
                          'My Devices',
                          style: theme.textTheme.headlineSmall?.copyWith(
                            fontWeight: FontWeight.bold,
                          ),
                        ),
                      ),
                      IconButton(
                        icon: const Icon(Icons.close),
                        onPressed: () => Navigator.of(context).pop(),
                      ),
                    ],
                  ),
                  const SizedBox(height: 12),
                  // Search bar
                  TextField(
                    decoration: InputDecoration(
                      hintText: 'Search devices...',
                      prefixIcon: const Icon(Icons.search, size: 20),
                      filled: true,
                      fillColor: isDark ? const Color(0xFF111827) : Colors.white,
                      border: OutlineInputBorder(
                        borderRadius: BorderRadius.circular(8),
                        borderSide: BorderSide(
                          color: isDark ? const Color(0xFF374151) : const Color(0xFFE5E7EB),
                        ),
                      ),
                      contentPadding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
                      isDense: true,
                    ),
                    onChanged: (value) {
                      setState(() {
                        _searchQuery = value.toLowerCase();
                      });
                    },
                  ),
                  const SizedBox(height: 8),
                  // Sort dropdown
                  Row(
                    children: [
                      Text(
                        'Sort by:',
                        style: theme.textTheme.bodySmall?.copyWith(
                          color: isDark ? const Color(0xFF9CA3AF) : const Color(0xFF6B7280),
                        ),
                      ),
                      const SizedBox(width: 8),
                      Expanded(
                        child: DropdownButton<String>(
                          value: _sortBy,
                          isExpanded: true,
                          underline: const SizedBox(),
                          items: const [
                            DropdownMenuItem(value: 'name', child: Text('Name')),
                            DropdownMenuItem(value: 'status', child: Text('Status')),
                            DropdownMenuItem(value: 'lastSeen', child: Text('Last Seen')),
                          ],
                          onChanged: (value) {
                            setState(() {
                              _sortBy = value ?? 'name';
                            });
                          },
                        ),
                      ),
                    ],
                  ),
                ],
              ),
            ),

            // Device list
            Expanded(
              child: Consumer<DeviceProvider>(
                builder: (context, deviceProvider, child) {
                  if (deviceProvider.isLoading) {
                    return const Center(child: CircularProgressIndicator());
                  }

                  if (deviceProvider.error != null) {
                    return Center(
                      child: Padding(
                        padding: const EdgeInsets.all(16.0),
                        child: Column(
                          mainAxisAlignment: MainAxisAlignment.center,
                          children: [
                            const Icon(Icons.error_outline, size: 48, color: Colors.red),
                            const SizedBox(height: 16),
                            Text(
                              deviceProvider.error!,
                              textAlign: TextAlign.center,
                              style: const TextStyle(color: Colors.red),
                            ),
                            const SizedBox(height: 16),
                            ElevatedButton(
                              onPressed: () => deviceProvider.fetchDevices(),
                              child: const Text('Retry'),
                            ),
                          ],
                        ),
                      ),
                    );
                  }

                  final devices = _getFilteredAndSortedDevices(deviceProvider.devices);

                  if (devices.isEmpty) {
                    return Center(
                      child: Padding(
                        padding: const EdgeInsets.all(16.0),
                        child: Column(
                          mainAxisAlignment: MainAxisAlignment.center,
                          children: [
                            Icon(
                              Icons.devices_other,
                              size: 64,
                              color: isDark ? const Color(0xFF374151) : const Color(0xFFE5E7EB),
                            ),
                            const SizedBox(height: 16),
                            Text(
                              _searchQuery.isEmpty
                                  ? 'No devices found'
                                  : 'No devices match your search',
                              style: theme.textTheme.bodyLarge?.copyWith(
                                color: isDark ? const Color(0xFF9CA3AF) : const Color(0xFF6B7280),
                              ),
                            ),
                          ],
                        ),
                      ),
                    );
                  }

                  return RefreshIndicator(
                    onRefresh: () => deviceProvider.fetchDevices(),
                    child: ListView.builder(
                      padding: const EdgeInsets.all(8),
                      itemCount: devices.length,
                      itemBuilder: (context, index) {
                        final device = devices[index];
                        return DeviceCardSidebar(
                          device: device,
                          onTap: () {
                            deviceProvider.setSelectedDevice(device);
                            Navigator.of(context).pop(); // Close drawer
                            Navigator.of(context).push(
                              MaterialPageRoute(
                                builder: (context) => const WaterTankControlScreen(),
                              ),
                            );
                          },
                          onSettingsTap: () {
                            Navigator.of(context).pop(); // Close drawer
                            Navigator.of(context).push(
                              MaterialPageRoute(
                                builder: (context) => DeviceSettingsScreen(device: device),
                              ),
                            );
                          },
                        );
                      },
                    ),
                  );
                },
              ),
            ),

            // Add device button
            Container(
              padding: const EdgeInsets.all(16),
              decoration: BoxDecoration(
                color: isDark ? const Color(0xFF1F2937) : const Color(0xFFF3F4F6),
                border: Border(
                  top: BorderSide(
                    color: isDark ? const Color(0xFF374151) : const Color(0xFFE5E7EB),
                  ),
                ),
              ),
              child: ElevatedButton.icon(
                onPressed: () {
                  Navigator.of(context).pop(); // Close drawer
                  Navigator.of(context).push(
                    MaterialPageRoute(
                      builder: (context) => const AddDeviceScreen(),
                    ),
                  );
                },
                icon: const Icon(Icons.add),
                label: const Text('Add New Device'),
                style: ElevatedButton.styleFrom(
                  minimumSize: const Size(double.infinity, 48),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  List<Device> _getFilteredAndSortedDevices(List<Device> devices) {
    // Filter by search query
    var filtered = devices.where((device) {
      if (_searchQuery.isEmpty) return true;
      return device.name.toLowerCase().contains(_searchQuery) ||
          device.deviceId.toLowerCase().contains(_searchQuery);
    }).toList();

    // Sort devices
    switch (_sortBy) {
      case 'status':
        filtered.sort((a, b) {
          if (a.isOnline && !b.isOnline) return -1;
          if (!a.isOnline && b.isOnline) return 1;
          return a.name.compareTo(b.name);
        });
        break;
      case 'lastSeen':
        filtered.sort((a, b) {
          final aTime = a.lastSeen ?? DateTime(1970);
          final bTime = b.lastSeen ?? DateTime(1970);
          return bTime.compareTo(aTime);
        });
        break;
      default: // name
        filtered.sort((a, b) => a.name.compareTo(b.name));
    }

    return filtered;
  }
}

class DeviceCardSidebar extends StatelessWidget {
  final Device device;
  final VoidCallback onTap;
  final VoidCallback onSettingsTap;

  const DeviceCardSidebar({
    Key? key,
    required this.device,
    required this.onTap,
    required this.onSettingsTap,
  }) : super(key: key);

  /// Convert string color to Color object
  Color _getColorFromString(String colorString) {
    switch (colorString.toLowerCase()) {
      case 'green':
        return const Color(0xFF10B981); // Green-500
      case 'yellow':
        return const Color(0xFFFBBF24); // Yellow-400
      case 'red':
        return const Color(0xFFEF4444); // Red-500
      case 'grey':
      default:
        return const Color(0xFF6B7280); // Gray-500
    }
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final isDark = theme.brightness == Brightness.dark;

    return Consumer<TimestampProvider>(
      builder: (context, timestampProvider, child) {
        // Format synced timestamp
        String? syncedTime;
        String? syncSource;

        if (timestampProvider.lastSyncSource == 'server' &&
            timestampProvider.lastServerSync != null) {
          final timestamp = DateTime.fromMillisecondsSinceEpoch(
            timestampProvider.lastServerSync!.serverTime,
          );
          syncedTime = DateFormat('HH:mm:ss').format(timestamp);
          syncSource = 'Server';
        } else if (timestampProvider.lastSyncSource == 'device' &&
                   timestampProvider.lastDeviceSync != null) {
          final timestamp = DateTime.fromMillisecondsSinceEpoch(
            timestampProvider.lastDeviceSync!.timestamp,
          );
          syncedTime = DateFormat('HH:mm:ss').format(timestamp);
          syncSource = timestampProvider.lastDeviceSync!.source == 'server'
              ? 'Device (synced)'
              : 'Device (local)';
        }

        return Container(
          margin: const EdgeInsets.only(bottom: 6),
          decoration: BoxDecoration(
            color: isDark ? const Color(0xFF1F2937) : Colors.white,
            border: Border.all(
              color: isDark ? const Color(0xFF374151) : const Color(0xFFE5E7EB),
            ),
            borderRadius: BorderRadius.circular(8),
          ),
          child: InkWell(
            onTap: onTap,
            borderRadius: BorderRadius.circular(8),
            child: Padding(
              padding: const EdgeInsets.all(10),
              child: Row(
                children: [
                  // Online/Offline indicator
                  Container(
                    width: 8,
                    height: 8,
                    decoration: BoxDecoration(
                      color: device.isOnline
                          ? const Color(0xFF10B981) // Green-500
                          : const Color(0xFF6B7280), // Gray-500
                      shape: BoxShape.circle,
                    ),
                  ),
                  const SizedBox(width: 10),

                  // Device info
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        Text(
                          device.name,
                          style: theme.textTheme.bodyMedium?.copyWith(
                            fontWeight: FontWeight.w600,
                            color: isDark ? Colors.white : const Color(0xFF111827),
                          ),
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                        ),
                        const SizedBox(height: 2),
                        Text(
                          '${device.deviceId} • ${device.lastSeenText}',
                          style: theme.textTheme.bodySmall?.copyWith(
                            color: isDark ? const Color(0xFF9CA3AF) : const Color(0xFF6B7280),
                            fontSize: 11,
                          ),
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                        ),
                        if (syncedTime != null && syncSource != null) ...[
                          const SizedBox(height: 2),
                          Row(
                            children: [
                              Icon(
                                Icons.access_time,
                                size: 10,
                                color: isDark ? const Color(0xFF6B7280) : const Color(0xFF9CA3AF),
                              ),
                              const SizedBox(width: 4),
                              Expanded(
                                child: Text(
                                  '$syncedTime • $syncSource',
                                  style: theme.textTheme.bodySmall?.copyWith(
                                    color: isDark ? const Color(0xFF6B7280) : const Color(0xFF9CA3AF),
                                    fontSize: 10,
                                  ),
                                  maxLines: 1,
                                  overflow: TextOverflow.ellipsis,
                                ),
                              ),
                            ],
                          ),
                        ],
                      ],
                    ),
                  ),

                  const SizedBox(width: 8),

                  // Action button
                  Material(
                    color: isDark ? const Color(0xFF374151) : const Color(0xFFF3F4F6),
                    borderRadius: BorderRadius.circular(6),
                    child: InkWell(
                      onTap: onSettingsTap,
                      borderRadius: BorderRadius.circular(6),
                      child: Padding(
                        padding: const EdgeInsets.all(6),
                        child: Icon(
                          Icons.settings,
                          size: 18,
                          color: isDark ? const Color(0xFF9CA3AF) : const Color(0xFF6B7280),
                        ),
                      ),
                    ),
                  ),
                ],
              ),
            ),
          ),
        );
      },
    );
  }
}
