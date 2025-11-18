import 'dart:async';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../models/device.dart';
import '../providers/device_provider.dart';
import '../providers/offline_provider.dart';
import '../widgets/loading_widget.dart';
import '../widgets/error_widget.dart';
import 'device_config_edit_screen.dart';

/// Water tank specific control screen with custom UI layout
class WaterTankControlScreen extends StatefulWidget {
  const WaterTankControlScreen({Key? key}) : super(key: key);

  @override
  State<WaterTankControlScreen> createState() => _WaterTankControlScreenState();
}

class _WaterTankControlScreenState extends State<WaterTankControlScreen>
    with SingleTickerProviderStateMixin {
  bool _isTogglingPump = false;
  Timer? _refreshTimer;
  DateTime _lastUpdate = DateTime.now();
  late AnimationController _pulseController;
  late Animation<double> _pulseAnimation;

  @override
  void initState() {
    super.initState();
    // Initialize pulse animation for LIVE indicator
    _pulseController = AnimationController(
      duration: const Duration(milliseconds: 1500),
      vsync: this,
    )..repeat(reverse: true);
    _pulseAnimation = Tween<double>(begin: 0.3, end: 1.0).animate(
      CurvedAnimation(parent: _pulseController, curve: Curves.easeInOut),
    );
    // Start auto-refresh timer (every 3 seconds)
    _startAutoRefresh();
  }

  @override
  void dispose() {
    // Cancel timer and animation when screen is disposed
    _refreshTimer?.cancel();
    _pulseController.dispose();
    super.dispose();
  }

  void _startAutoRefresh() {
    _refreshTimer = Timer.periodic(const Duration(seconds: 3), (timer) async {
      final deviceProvider = context.read<DeviceProvider>();
      final offlineProvider = context.read<OfflineProvider>();

      if (deviceProvider.selectedDevice != null && !deviceProvider.isLoading) {
        try {
          // Get local IP from device config
          final localIp = deviceProvider.selectedDevice!.deviceConfig['ip_address']?.value;
          final deviceId = deviceProvider.selectedDevice!.id;

          // Try to get data using offline service (local first, then server)
          final device = await offlineProvider.service.getDevice(
            deviceId,
            localIp: localIp?.toString(),
          );

          // Update the provider with new data
          deviceProvider.setSelectedDevice(device);

          if (mounted) {
            setState(() {
              _lastUpdate = DateTime.now();
            });
          }
        } catch (e) {
          print('Auto-refresh failed: $e');
          // Fall back to regular provider method
          deviceProvider.selectDevice(deviceProvider.selectedDevice!.id).then((_) {
            if (mounted) {
              setState(() {
                _lastUpdate = DateTime.now();
              });
            }
          });
        }
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Theme.of(context).colorScheme.surface,
      body: Consumer<DeviceProvider>(
        builder: (context, deviceProvider, child) {
          final device = deviceProvider.selectedDevice;

          // Loading state
          if (deviceProvider.isLoading && device == null) {
            return const LoadingWidget(message: 'Loading device...');
          }

          // Error state
          if (deviceProvider.error != null && device == null) {
            return ErrorDisplayWidget(
              message: deviceProvider.error!,
              onRetry: () {
                if (deviceProvider.selectedDevice != null) {
                  deviceProvider.selectDevice(deviceProvider.selectedDevice!.id);
                }
              },
            );
          }

          // No device selected
          if (device == null) {
            return const Center(child: Text('No device selected'));
          }

          return RefreshIndicator(
            onRefresh: () async {
              await deviceProvider.selectDevice(device.id);
            },
            child: _buildWaterTankUI(context, device, deviceProvider),
          );
        },
      ),
    );
  }

  Widget _buildWaterTankUI(
    BuildContext context,
    Device device,
    DeviceProvider deviceProvider,
  ) {
    // Extract telemetry data
    final waterLevel = device.telemetryData['waterLevel']?.numberValue ?? 0.0;
    final currInflow = device.telemetryData['currInflow']?.numberValue ?? 0.0;
    final pumpStatus = device.telemetryData['pumpStatus']?.numberValue ?? 0.0;

    // Extract device config - convert to double to avoid type errors
    final upperThreshold = _toDouble(device.deviceConfig['upperThreshold']?.value);
    final lowerThreshold = _toDouble(device.deviceConfig['lowerThreshold']?.value);
    final usedTotal = _toDouble(device.deviceConfig['UsedTotal']?.value);
    final maxInflow = _toDouble(device.deviceConfig['maxInflow']?.value);

    // Extract control data
    final pumpSwitch = device.controlData['pumpSwitch']?.value ?? false;
    final isOnline = device.isActive;

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        // Custom Header
        _buildHeader(context, device, isOnline),
        const SizedBox(height: 24),

        // Water Level Label
        Text(
          'Water Level',
          style: Theme.of(context).textTheme.titleMedium?.copyWith(
                fontWeight: FontWeight.w500,
              ),
        ),
        const SizedBox(height: 16),

        // Water Level Visual Indicator
        _buildWaterLevelIndicator(context, waterLevel, upperThreshold, lowerThreshold),
        const SizedBox(height: 32),

        // Data Grid - Row 1: Thresholds
        Row(
          children: [
            Expanded(
              child: _buildDataCard(
                context,
                'Upper Threshold',
                '${upperThreshold.toStringAsFixed(0)}%',
              ),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: _buildDataCard(
                context,
                'Lower Threshold',
                '${lowerThreshold.toStringAsFixed(0)}%',
              ),
            ),
          ],
        ),
        const SizedBox(height: 12),

        // Data Grid - Row 2: Total Water Used & Pump Status
        Row(
          children: [
            Expanded(
              child: _buildDataCard(
                context,
                'Total Water Used',
                '${usedTotal.toStringAsFixed(0)}L',
              ),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: _buildDataCard(
                context,
                'Pump Status',
                pumpStatus > 0 ? 'ON' : 'OFF',
                valueColor: pumpStatus > 0 ? Colors.green : Colors.grey,
              ),
            ),
          ],
        ),
        const SizedBox(height: 12),

        // Data Grid - Row 3: Inflow & Max Inflow
        Row(
          children: [
            Expanded(
              child: _buildDataCard(
                context,
                'Inflow',
                '${currInflow.toStringAsFixed(1)}Lpm',
              ),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: _buildDataCard(
                context,
                'Max Inflow',
                '${maxInflow.toStringAsFixed(1)}Lpm',
              ),
            ),
          ],
        ),
        const SizedBox(height: 40),

        // Pump Control Button
        Center(
          child: _buildPumpControlButton(
            context,
            pumpSwitch as bool,
            deviceProvider,
            device,
          ),
        ),
        const SizedBox(height: 24),
      ],
    );
  }

  Widget _buildHeader(BuildContext context, Device device, bool isOnline) {
    return Consumer<OfflineProvider>(
      builder: (context, offlineProvider, child) {
        return Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                // Back button
                IconButton(
                  icon: const Icon(Icons.arrow_back),
                  onPressed: () => Navigator.pop(context),
                ),
                const SizedBox(width: 8),

                // Device ID and status
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        'Device ID:',
                        style: Theme.of(context).textTheme.bodySmall?.copyWith(
                              color: Theme.of(context).colorScheme.onSurfaceVariant,
                            ),
                      ),
                      const SizedBox(height: 2),
                      Row(
                        children: [
                          Flexible(
                            child: Text(
                              device.deviceId,
                              style: Theme.of(context).textTheme.titleSmall?.copyWith(
                                    fontWeight: FontWeight.bold,
                                  ),
                              overflow: TextOverflow.ellipsis,
                            ),
                          ),
                          const SizedBox(width: 8),
                          Container(
                            padding: const EdgeInsets.symmetric(
                              horizontal: 8,
                              vertical: 4,
                            ),
                            decoration: BoxDecoration(
                              color: isOnline ? Colors.green : Colors.grey,
                              borderRadius: BorderRadius.circular(12),
                            ),
                            child: Row(
                              mainAxisSize: MainAxisSize.min,
                              children: [
                                if (isOnline) ...[
                                  FadeTransition(
                                    opacity: _pulseAnimation,
                                    child: Container(
                                      width: 6,
                                      height: 6,
                                      decoration: const BoxDecoration(
                                        color: Colors.white,
                                        shape: BoxShape.circle,
                                      ),
                                    ),
                                  ),
                                  const SizedBox(width: 4),
                                ],
                                Text(
                                  isOnline ? 'LIVE' : 'Offline',
                                  style: const TextStyle(
                                    color: Colors.white,
                                    fontSize: 10,
                                    fontWeight: FontWeight.bold,
                                  ),
                                ),
                              ],
                            ),
                          ),
                        ],
                      ),
                    ],
                  ),
                ),

                // Sync button (if pending changes)
                if (offlineProvider.pendingChanges > 0)
                  IconButton(
                    icon: Stack(
                      children: [
                        const Icon(Icons.sync),
                        Positioned(
                          right: 0,
                          top: 0,
                          child: Container(
                            padding: const EdgeInsets.all(2),
                            decoration: BoxDecoration(
                              color: Colors.orange,
                              borderRadius: BorderRadius.circular(6),
                            ),
                            constraints: const BoxConstraints(
                              minWidth: 12,
                              minHeight: 12,
                            ),
                            child: Text(
                              '${offlineProvider.pendingChanges}',
                              style: const TextStyle(
                                color: Colors.white,
                                fontSize: 8,
                                fontWeight: FontWeight.bold,
                              ),
                              textAlign: TextAlign.center,
                            ),
                          ),
                        ),
                      ],
                    ),
                    onPressed: offlineProvider.isSyncing
                        ? null
                        : () async {
                            await offlineProvider.syncPendingChanges();
                            if (mounted) {
                              ScaffoldMessenger.of(context).showSnackBar(
                                SnackBar(
                                  content: Text(
                                    offlineProvider.lastSyncResult?.allSynced == true
                                        ? 'Synced ${offlineProvider.lastSyncResult!.synced} change(s)'
                                        : 'Sync completed with ${offlineProvider.lastSyncResult?.failed ?? 0} failure(s)',
                                  ),
                                  backgroundColor: offlineProvider.lastSyncResult?.hasFailures == true
                                      ? Colors.orange
                                      : Colors.green,
                                ),
                              );
                            }
                          },
                    tooltip: 'Sync ${offlineProvider.pendingChanges} pending change(s)',
                  ),

                // Info button
                IconButton(
                  icon: const Icon(Icons.info_outline),
                  onPressed: () => _showDeviceInfoDialog(context, device),
                ),

                // Settings button
                IconButton(
                  icon: const Icon(Icons.settings),
                  onPressed: () async {
                    // Show loading dialog
                    showDialog(
                      context: context,
                      barrierDismissible: false,
                      builder: (context) => const Center(
                        child: Card(
                          child: Padding(
                            padding: EdgeInsets.all(24.0),
                            child: Column(
                              mainAxisSize: MainAxisSize.min,
                              children: [
                                CircularProgressIndicator(),
                                SizedBox(height: 16),
                                Text('Fetching device configuration...'),
                              ],
                            ),
                          ),
                        ),
                      ),
                    );

                    try {
                      // Fetch fresh device data from server
                      final deviceProvider = context.read<DeviceProvider>();
                      await deviceProvider.selectDevice(device.id);

                      // Close loading dialog
                      if (mounted) Navigator.of(context).pop();

                      // Get the updated device
                      final updatedDevice = deviceProvider.selectedDevice;

                      if (updatedDevice != null && mounted) {
                        // Navigate to config screen with fresh data
                        Navigator.push(
                          context,
                          MaterialPageRoute(
                            builder: (context) => DeviceConfigEditScreen(device: updatedDevice),
                          ),
                        );
                      }
                    } catch (e) {
                      // Close loading dialog
                      if (mounted) Navigator.of(context).pop();

                      // Show error message
                      if (mounted) {
                        ScaffoldMessenger.of(context).showSnackBar(
                          SnackBar(
                            content: Text('Failed to fetch device config: $e'),
                            backgroundColor: Colors.red,
                            duration: const Duration(seconds: 3),
                          ),
                        );
                      }
                    }
                  },
                ),
              ],
            ),

            // Network status indicator
            if (!offlineProvider.isOnline)
              Container(
                margin: const EdgeInsets.only(top: 8),
                padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
                decoration: BoxDecoration(
                  color: Colors.orange.withOpacity(0.2),
                  borderRadius: BorderRadius.circular(8),
                  border: Border.all(color: Colors.orange),
                ),
                child: Row(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    const Icon(Icons.cloud_off, size: 16, color: Colors.orange),
                    const SizedBox(width: 8),
                    Text(
                      offlineProvider.pendingChanges > 0
                          ? 'Offline - ${offlineProvider.pendingChanges} change(s) pending'
                          : 'Offline mode - using cached data',
                      style: const TextStyle(
                        color: Colors.orange,
                        fontSize: 12,
                        fontWeight: FontWeight.w500,
                      ),
                    ),
                  ],
                ),
              ),
          ],
        );
      },
    );
  }

  Widget _buildWaterLevelIndicator(
    BuildContext context,
    double waterLevel,
    double upperThreshold,
    double lowerThreshold,
  ) {
    // Calculate percentage (0-100)
    final percentage = waterLevel.clamp(0.0, 100.0);

    return Center(
      child: SizedBox(
        width: 100,
        height: 250,
        child: Stack(
          alignment: Alignment.bottomCenter,
          children: [
            // Container border
            Container(
              decoration: BoxDecoration(
                border: Border.all(
                  color: Theme.of(context).colorScheme.outline,
                  width: 2,
                ),
                borderRadius: BorderRadius.circular(8),
              ),
            ),
            // Water fill
            FractionallySizedBox(
              heightFactor: percentage / 100,
              alignment: Alignment.bottomCenter,
              child: Container(
                decoration: BoxDecoration(
                  color: _getWaterLevelColor(percentage, upperThreshold, lowerThreshold),
                  borderRadius: BorderRadius.circular(6),
                ),
              ),
            ),
            // Percentage text
            Center(
              child: Container(
                padding: const EdgeInsets.symmetric(
                  horizontal: 12,
                  vertical: 6,
                ),
                decoration: BoxDecoration(
                  color: Theme.of(context).colorScheme.surface.withOpacity(0.9),
                  borderRadius: BorderRadius.circular(8),
                ),
                child: Text(
                  '${percentage.toStringAsFixed(0)}%',
                  style: Theme.of(context).textTheme.titleLarge?.copyWith(
                        fontWeight: FontWeight.bold,
                      ),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Color _getWaterLevelColor(double level, double upper, double lower) {
    if (level >= upper) {
      return Colors.green.shade400;
    } else if (level <= lower) {
      return Colors.red.shade400;
    } else {
      return Colors.blue.shade400;
    }
  }

  Widget _buildDataCard(
    BuildContext context,
    String label,
    String value, {
    Color? valueColor,
  }) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              label,
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                    color: Theme.of(context).colorScheme.onSurfaceVariant,
                  ),
            ),
            const SizedBox(height: 8),
            Text(
              value,
              style: Theme.of(context).textTheme.headlineSmall?.copyWith(
                    fontWeight: FontWeight.bold,
                    color: valueColor,
                  ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildPumpControlButton(
    BuildContext context,
    bool pumpSwitch,
    DeviceProvider deviceProvider,
    Device device,
  ) {
    return SizedBox(
      width: 200,
      height: 200,
      child: Material(
        color: pumpSwitch ? Colors.green : Colors.grey.shade300,
        borderRadius: BorderRadius.circular(16),
        elevation: 4,
        child: InkWell(
          onTap: _isTogglingPump
              ? null
              : () async {
                  setState(() => _isTogglingPump = true);

                  final offlineProvider = context.read<OfflineProvider>();
                  final localIp = device.deviceConfig['ip_address']?.value;

                  try {
                    // Use offline service (tries local first, then server)
                    await offlineProvider.service.updateControl(
                      device.id,
                      'pumpSwitch',
                      !pumpSwitch,
                      'boolean',
                      localIp: localIp?.toString(),
                    );

                    // Immediately refresh to get updated state
                    await deviceProvider.selectDevice(device.id);

                    setState(() => _isTogglingPump = false);

                    // Show sync status if offline
                    if (!offlineProvider.isOnline && mounted) {
                      ScaffoldMessenger.of(context).showSnackBar(
                        const SnackBar(
                          content: Text('Command queued for sync (offline mode)'),
                          duration: Duration(seconds: 2),
                        ),
                      );
                    }
                  } catch (e) {
                    setState(() => _isTogglingPump = false);

                    if (mounted) {
                      ScaffoldMessenger.of(context).showSnackBar(
                        SnackBar(
                          content: Text('Failed to toggle pump: $e'),
                          backgroundColor: Theme.of(context).colorScheme.error,
                        ),
                      );
                    }
                  }
                },
          borderRadius: BorderRadius.circular(16),
          child: Center(
            child: _isTogglingPump
                ? CircularProgressIndicator(
                    color: pumpSwitch ? Colors.white : Colors.grey.shade700,
                  )
                : Text(
                    pumpSwitch ? 'ON' : 'OFF',
                    style: TextStyle(
                      fontSize: 48,
                      fontWeight: FontWeight.bold,
                      color: pumpSwitch ? Colors.white : Colors.grey.shade700,
                    ),
                  ),
          ),
        ),
      ),
    );
  }

  void _showDeviceInfoDialog(BuildContext context, Device device) {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Device Information'),
        content: SingleChildScrollView(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            mainAxisSize: MainAxisSize.min,
            children: [
              _buildInfoRow('Device ID', device.deviceId),
              _buildInfoRow('Name', device.name),
              if (device.projectName != null)
                _buildInfoRow('Project', device.projectName!),
              _buildInfoRow('Status', device.isActive ? 'Online' : 'Offline'),
              _buildInfoRow('Last Seen', device.getStatusText()),
              if (device.deviceConfig['ip_address']?.value != null &&
                  device.deviceConfig['ip_address']!.value.toString().isNotEmpty)
                _buildInfoRow(
                  'Local IP',
                  device.deviceConfig['ip_address']!.value.toString(),
                ),
              const SizedBox(height: 16),
              Text(
                'Tank Configuration',
                style: Theme.of(context).textTheme.titleSmall?.copyWith(
                      fontWeight: FontWeight.bold,
                    ),
              ),
              const SizedBox(height: 8),
              if (device.deviceConfig['tankShape'] != null)
                _buildInfoRow(
                  'Tank Shape',
                  device.deviceConfig['tankShape']!.value.toString(),
                ),
              if (device.deviceConfig['tankHeight'] != null)
                _buildInfoRow(
                  'Tank Height',
                  '${device.deviceConfig['tankHeight']!.value} cm',
                ),
              if (device.deviceConfig['tankWidth'] != null)
                _buildInfoRow(
                  'Tank Width',
                  '${device.deviceConfig['tankWidth']!.value} cm',
                ),
            ],
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Close'),
          ),
        ],
      ),
    );
  }

  Widget _buildInfoRow(String label, String value) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SizedBox(
            width: 100,
            child: Text(
              '$label:',
              style: const TextStyle(fontWeight: FontWeight.w500),
            ),
          ),
          Expanded(
            child: Text(
              value,
              style: const TextStyle(color: Colors.grey),
            ),
          ),
        ],
      ),
    );
  }

  /// Helper method to safely convert dynamic values to double
  double _toDouble(dynamic value) {
    if (value == null) return 0.0;
    if (value is double) return value;
    if (value is int) return value.toDouble();
    if (value is num) return value.toDouble();
    // Try to parse string
    if (value is String) {
      return double.tryParse(value) ?? 0.0;
    }
    return 0.0;
  }
}
