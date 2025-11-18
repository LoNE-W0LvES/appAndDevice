import 'dart:async';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../models/device.dart';
import '../providers/device_provider.dart';
import '../providers/offline_provider.dart';
import '../providers/theme_provider.dart' as app_theme;
import '../providers/timestamp_provider.dart';
import '../widgets/loading_widget.dart';
import '../widgets/error_widget.dart';
import '../widgets/circular_water_level.dart';
import '../widgets/theme_toggle.dart';
import '../widgets/threshold_range_card.dart';
import '../widgets/device_sidebar.dart';
import '../config/app_config.dart';
import 'device_config_edit_screen.dart';
import 'device_settings_screen.dart';

/// Redesigned Water Tank Control Screen with theme support
class WaterTankControlScreen extends StatefulWidget {
  const WaterTankControlScreen({Key? key}) : super(key: key);

  @override
  State<WaterTankControlScreen> createState() => _WaterTankControlScreenState();
}

class _WaterTankControlScreenState extends State<WaterTankControlScreen>
    with SingleTickerProviderStateMixin, WidgetsBindingObserver {
  bool _isTogglingPump = false;
  Timer? _refreshTimer;
  DateTime _lastUpdate = DateTime.now();
  late AnimationController _pulseController;
  late Animation<double> _pulseAnimation;

  @override
  void initState() {
    super.initState();
    // Add observer for app lifecycle changes
    WidgetsBinding.instance.addObserver(this);

    // Initialize pulse animation for LIVE indicator
    _pulseController = AnimationController(
      duration: const Duration(milliseconds: 1500),
      vsync: this,
    )..repeat(reverse: true);
    _pulseAnimation = Tween<double>(begin: 0.3, end: 1.0).animate(
      CurvedAnimation(parent: _pulseController, curve: Curves.easeInOut),
    );
    // Sync timestamp when screen loads (schedule after build completes)
    WidgetsBinding.instance.addPostFrameCallback((_) {
      _syncTimestamp();
    });
    // Start auto-refresh timer (every 3 seconds)
    _startAutoRefresh();
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    // Pause/resume timer based on app state
    if (state == AppLifecycleState.paused || state == AppLifecycleState.inactive) {
      // App is in background, pause timer to save resources
      _refreshTimer?.cancel();
    } else if (state == AppLifecycleState.resumed) {
      // App is back in foreground, restart timer if screen is mounted
      if (mounted && _refreshTimer == null || !(_refreshTimer?.isActive ?? false)) {
        _startAutoRefresh();
      }
    }
  }

  /// Sync timestamp with device
  Future<void> _syncTimestamp() async {
    final deviceProvider = context.read<DeviceProvider>();
    final timestampProvider = context.read<TimestampProvider>();
    final offlineProvider = context.read<OfflineProvider>();

    if (deviceProvider.selectedDevice != null) {
      final device = deviceProvider.selectedDevice!;
      // Get local IP from device model (stored for offline mode)
      final localIp = device.localIp;

      // Check if phone time is valid (after November 2024)
      final phoneTime = DateTime.now();
      final minValidTime = DateTime(2024, 11, 1); // November 2024

      if (phoneTime.isBefore(minValidTime)) {
        // Phone time is in the past - show warning
        if (mounted) {
          _showInvalidTimeWarning();
        }
        return; // Don't sync with invalid phone time
      }

      if (localIp != null && localIp.isNotEmpty) {
        // In offline mode, sync with device using phone's Unix time
        if (offlineProvider.isOfflineModeEnabled) {
          AppConfig.offlineLog('Offline mode: Syncing timestamp with device only');
          await timestampProvider.syncDevice(
            device.deviceId,
            localIp: localIp,
          );
        } else {
          // Online mode: sync with both server and device
          await timestampProvider.syncDevice(
            device.deviceId,
            localIp: localIp,
          );
        }
      }
    }
  }

  /// Show warning dialog for invalid phone time
  void _showInvalidTimeWarning() {
    showDialog(
      context: context,
      barrierDismissible: false,
      builder: (context) => AlertDialog(
        title: Row(
          children: [
            Icon(Icons.warning_amber_rounded, color: Colors.orange, size: 28),
            SizedBox(width: 12),
            Text('Invalid Phone Time'),
          ],
        ),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              'Your phone\'s date/time appears to be incorrect.',
              style: TextStyle(fontWeight: FontWeight.bold),
            ),
            SizedBox(height: 12),
            Text('Current phone time: ${DateTime.now().toString().split('.')[0]}'),
            SizedBox(height: 8),
            Text('Please update your phone\'s:'),
            SizedBox(height: 8),
            Text('• Date and Time'),
            Text('• Timezone'),
            Text('• Enable "Set time automatically"'),
            SizedBox(height: 12),
            Text(
              'This is required for accurate device synchronization.',
              style: TextStyle(fontSize: 13, color: Colors.grey[600]),
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: Text('OK'),
          ),
        ],
      ),
    );
  }

  @override
  void dispose() {
    // Remove lifecycle observer
    WidgetsBinding.instance.removeObserver(this);
    _refreshTimer?.cancel();
    _pulseController.dispose();
    super.dispose();
  }

  void _startAutoRefresh() {
    _refreshTimer = Timer.periodic(const Duration(seconds: 3), (timer) async {
      // Only fetch data if this screen is the currently visible route
      // This prevents unnecessary data fetching when user navigates to other screens
      if (!mounted) return;

      final isCurrentRoute = ModalRoute.of(context)?.isCurrent ?? false;
      if (!isCurrentRoute) {
        // User has navigated to another screen (e.g., device settings, config editor)
        // Skip this fetch cycle to save resources and avoid unnecessary requests
        return;
      }

      final deviceProvider = context.read<DeviceProvider>();
      final offlineProvider = context.read<OfflineProvider>();
      final timestampProvider = context.read<TimestampProvider>();

      if (deviceProvider.selectedDevice != null && !deviceProvider.isLoading) {
        try {
          // Get local IP from device model (stored for offline mode)
          final localIp = deviceProvider.selectedDevice!.localIp;
          final deviceId = deviceProvider.selectedDevice!.id;

          // Periodic timestamp sync (every hour)
          if (timestampProvider.needsSync()) {
            await timestampProvider.syncDevice(
              deviceProvider.selectedDevice!.deviceId,
              localIp: localIp?.toString(),
            );
          }

          // Use getDeviceLiveData for efficient updates (telemetry + control only)
          final device = await offlineProvider.service.getDeviceLiveData(
            deviceId,
            localIp: localIp?.toString(),
          );

          deviceProvider.setSelectedDevice(device);

          if (mounted) {
            setState(() {
              _lastUpdate = DateTime.now();
            });
          }
        } catch (e) {
          AppConfig.deviceLog('Auto-refresh failed: $e');
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
    final themeProvider = context.watch<app_theme.ThemeProvider>();
    final isDarkMode = themeProvider.isDarkMode;

    return Scaffold(
      drawer: const DeviceSidebar(),
      body: Container(
        decoration: _buildGradientBackground(isDarkMode),
        child: SafeArea(
          child: Consumer<DeviceProvider>(
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
                child: _buildWaterTankUI(context, device, deviceProvider, isDarkMode),
              );
            },
          ),
        ),
      ),
    );
  }

  /// Build gradient background based on theme
  BoxDecoration _buildGradientBackground(bool isDarkMode) {
    if (isDarkMode) {
      return const BoxDecoration(
        gradient: LinearGradient(
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
          colors: [
            Color(0xFF1F2937), // gray-800
            Color(0xFF000000), // black
          ],
        ),
      );
    } else {
      return const BoxDecoration(
        gradient: LinearGradient(
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
          colors: [
            Color(0xFFFFFFFF), // white
            Color(0xFFECFEFF), // cyan-50
          ],
        ),
      );
    }
  }

  Widget _buildWaterTankUI(
    BuildContext context,
    Device device,
    DeviceProvider deviceProvider,
    bool isDarkMode,
  ) {
    // Extract telemetry data
    final waterLevel = device.telemetryData['waterLevel']?.numberValue ?? 0.0;
    final currInflow = device.telemetryData['currInflow']?.numberValue ?? 0.0;
    final pumpStatus = device.telemetryData['pumpStatus']?.numberValue ?? 0.0;

    // Extract device config
    final upperThreshold = _toDouble(device.deviceConfig['upperThreshold']?.value);
    final lowerThreshold = _toDouble(device.deviceConfig['lowerThreshold']?.value);
    final usedTotal = _toDouble(device.deviceConfig['UsedTotal']?.value);
    final maxInflow = _toDouble(device.deviceConfig['maxInflow']?.value);

    // Extract control data
    final pumpSwitch = device.controlData['pumpSwitch']?.value ?? false; // Manual switch control
    final isPumpOn = pumpStatus > 0; // Actual pump status from telemetry
    final isOnline = device.isOnline; // Device online status from telemetry

    return ListView(
      padding: const EdgeInsets.all(20),
      children: [
        // Header with Logo, Title, Device ID, LIVE badge, Theme toggle, Settings
        _buildModernHeader(context, device, isOnline, isDarkMode),
        const SizedBox(height: 32),

        // Circular Water Level with Wave Animation
        Center(
          child: CircularWaterLevel(
            percentage: waterLevel,
            size: 280,
            isDarkMode: isDarkMode,
            isPumpOn: isPumpOn,
            lowerThreshold: lowerThreshold,
            upperThreshold: upperThreshold,
          ),
        ),
        const SizedBox(height: 24),

        // Threshold Range Card with animated bar indicator
        ThresholdRangeCard(
          waterLevel: waterLevel,
          lowerThreshold: lowerThreshold,
          upperThreshold: upperThreshold,
          isDarkMode: isDarkMode,
        ),
        const SizedBox(height: 8),

        // Water Used and Inflow side by side
        Row(
          children: [
            Expanded(
              child: _buildMetricCard(
                context,
                'Water Used',
                '${usedTotal.toStringAsFixed(0)}L',
                Icons.water_drop,
                isDarkMode,
              ),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: _buildMetricCard(
                context,
                'Inflow',
                '${currInflow.toStringAsFixed(1)} L/min',
                Icons.water,
                isDarkMode,
              ),
            ),
          ],
        ),
        const SizedBox(height: 16),

        // Pump Control Button
        _buildPumpControlButton(context, device, pumpSwitch, isDarkMode),
        const SizedBox(height: 20),
      ],
    );
  }

  /// Modern header with mode indicator, online badge, theme toggle, and settings
  Widget _buildModernHeader(BuildContext context, Device device, bool isOnline, bool isDarkMode) {
    final colorScheme = Theme.of(context).colorScheme;

    // Determine if using webserver (local) or API (cloud) mode
    final isWebserverMode = device.localIp != null && device.localIp!.isNotEmpty;

    return Row(
      children: [
        // Menu button to open drawer (moved left with reduced padding)
        Padding(
          padding: const EdgeInsets.only(left: 0),
          child: Builder(
            builder: (context) => IconButton(
              icon: const Icon(Icons.menu, size: 24),
              onPressed: () => Scaffold.of(context).openDrawer(),
              tooltip: 'Device Management',
              padding: const EdgeInsets.all(8),
            ),
          ),
        ),

        const SizedBox(width: 4),

        // Mode indicator flag
        Container(
          padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
          decoration: BoxDecoration(
            color: isWebserverMode
                ? Colors.blue.withOpacity(0.15)
                : Colors.purple.withOpacity(0.15),
            borderRadius: BorderRadius.circular(12),
            border: Border.all(
              color: isWebserverMode
                  ? Colors.blue.withOpacity(0.5)
                  : Colors.purple.withOpacity(0.5),
              width: 1.5,
            ),
          ),
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              Icon(
                isWebserverMode ? Icons.router : Icons.cloud,
                size: 14,
                color: isWebserverMode ? Colors.blue : Colors.purple,
              ),
              const SizedBox(width: 4),
              Text(
                isWebserverMode ? 'LOCAL' : 'API',
                style: TextStyle(
                  color: isWebserverMode ? Colors.blue : Colors.purple,
                  fontWeight: FontWeight.bold,
                  fontSize: 10,
                  letterSpacing: 0.5,
                ),
              ),
            ],
          ),
        ),

        const Spacer(),

        // Online/Offline Status Badge
        AnimatedBuilder(
          animation: _pulseAnimation,
          builder: (context, child) {
            final statusColor = device.isOnline ? Colors.green : Colors.red;
            final statusText = device.isOnline ? 'ONLINE' : 'OFFLINE';

            return Container(
              padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
              decoration: BoxDecoration(
                color: statusColor.withOpacity(0.2 + (device.isOnline ? _pulseAnimation.value * 0.2 : 0)),
                borderRadius: BorderRadius.circular(20),
                border: Border.all(
                  color: statusColor.withOpacity(device.isOnline ? _pulseAnimation.value : 0.7),
                  width: 2,
                ),
              ),
              child: Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Container(
                    width: 8,
                    height: 8,
                    decoration: BoxDecoration(
                      color: statusColor,
                      shape: BoxShape.circle,
                      boxShadow: device.isOnline ? [
                        BoxShadow(
                          color: statusColor.withOpacity(_pulseAnimation.value),
                          blurRadius: 8,
                          spreadRadius: 2,
                        ),
                      ] : null,
                    ),
                  ),
                  const SizedBox(width: 6),
                  Text(
                    statusText,
                    style: TextStyle(
                      color: statusColor,
                      fontWeight: FontWeight.bold,
                      fontSize: 12,
                    ),
                  ),
                ],
              ),
            );
          },
        ),
        const SizedBox(width: 8),

        // Theme Toggle
        const ThemeToggle(),
        const SizedBox(width: 4),

        // Settings Button
        IconButton(
          icon: const Icon(Icons.settings),
          onPressed: () {
            Navigator.push(
              context,
              MaterialPageRoute(
                builder: (context) => DeviceSettingsScreen(device: device),
              ),
            );
          },
        ),
      ],
    );
  }

  /// Build theme-aware metric card
  Widget _buildMetricCard(
    BuildContext context,
    String label,
    String value,
    IconData icon,
    bool isDarkMode, {
    double? progress,
    Color? statusColor,
  }) {
    final colorScheme = Theme.of(context).colorScheme;
    final cardColor = isDarkMode ? const Color(0xFF1F2937) : const Color(0xFFFFFFFF);
    final borderColor = isDarkMode ? const Color(0xFF374151) : const Color(0xFFE5E7EB);

    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: cardColor,
        borderRadius: BorderRadius.circular(20),
        border: Border.all(color: borderColor, width: 1),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Icon(
                icon,
                color: statusColor ?? colorScheme.primary,
                size: 24,
              ),
              const SizedBox(width: 12),
              Expanded(
                child: Text(
                  label,
                  style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                        color: colorScheme.onSurface.withOpacity(0.7),
                      ),
                ),
              ),
            ],
          ),
          const SizedBox(height: 12),
          Text(
            value,
            style: Theme.of(context).textTheme.headlineMedium?.copyWith(
                  fontWeight: FontWeight.bold,
                  color: statusColor ?? colorScheme.onSurface,
                ),
          ),
          if (progress != null) ...[
            const SizedBox(height: 12),
            ClipRRect(
              borderRadius: BorderRadius.circular(8),
              child: LinearProgressIndicator(
                value: progress,
                backgroundColor: borderColor,
                valueColor: AlwaysStoppedAnimation<Color>(colorScheme.primary),
                minHeight: 6,
              ),
            ),
          ],
        ],
      ),
    );
  }

  /// Build large circular pump control button
  Widget _buildPumpControlButton(
    BuildContext context,
    Device device,
    bool pumpSwitch,
    bool isDarkMode,
  ) {
    final colorScheme = Theme.of(context).colorScheme;

    return Center(
      child: GestureDetector(
        onTap: _isTogglingPump
            ? null
            : () async {
                setState(() => _isTogglingPump = true);

                final offlineProvider = context.read<OfflineProvider>();
                final deviceProvider = context.read<DeviceProvider>();
                // Get local IP from device model (stored for offline mode)
                final localIp = device.localIp;

                try {
                  await offlineProvider.service.updateControl(
                    device.id,
                    'pumpSwitch',
                    !pumpSwitch,
                    'boolean',
                    localIp: localIp?.toString(),
                  );

                  // Immediately fetch updated device data for instant UI update
                  try {
                    final updatedDevice = await offlineProvider.service.getDeviceLiveData(
                      device.id,
                      localIp: localIp?.toString(),
                    );
                    deviceProvider.setSelectedDevice(updatedDevice);

                    if (mounted) {
                      setState(() {
                        _lastUpdate = DateTime.now();
                      });
                    }
                  } catch (e) {
                    AppConfig.deviceLog('Failed to fetch updated data after pump toggle: $e');
                  }

                  if (!offlineProvider.isOnline && mounted) {
                    ScaffoldMessenger.of(context).showSnackBar(
                      const SnackBar(
                        content: Text('Command queued for sync (offline mode)'),
                        duration: Duration(seconds: 2),
                      ),
                    );
                  }
                } catch (e) {
                  if (mounted) {
                    ScaffoldMessenger.of(context).showSnackBar(
                      SnackBar(
                        content: Text('Failed to toggle pump: $e'),
                        backgroundColor: Colors.red,
                      ),
                    );
                  }
                } finally {
                  if (mounted) {
                    setState(() => _isTogglingPump = false);
                  }
                }
              },
        child: Container(
          width: double.infinity,
          height: 70,
          decoration: BoxDecoration(
            borderRadius: BorderRadius.circular(16),
            color: pumpSwitch ? colorScheme.primary : colorScheme.surface,
            border: Border.all(
              color: pumpSwitch ? colorScheme.primary : colorScheme.outline,
              width: 3,
            ),
            boxShadow: pumpSwitch && isDarkMode
                ? [
                    BoxShadow(
                      color: colorScheme.primary.withOpacity(0.5),
                      blurRadius: 30,
                      spreadRadius: 5,
                    ),
                  ]
                : null,
          ),
          child: _isTogglingPump
              ? Center(
                  child: CircularProgressIndicator(
                    color: pumpSwitch ? Colors.black : colorScheme.primary,
                  ),
                )
              : Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    Icon(
                      pumpSwitch ? Icons.power_settings_new : Icons.power_off,
                      size: 32,
                      color: pumpSwitch ? Colors.black : colorScheme.primary,
                    ),
                    const SizedBox(width: 16),
                    Text(
                      pumpSwitch ? 'PUMP ON' : 'PUMP OFF',
                      style: TextTheme().titleLarge?.copyWith(
                            fontWeight: FontWeight.bold,
                            fontSize: 20,
                            color: pumpSwitch ? Colors.black : colorScheme.onSurface,
                          ),
                    ),
                  ],
                ),
        ),
      ),
    );
  }

  /// Helper to safely convert to double
  double _toDouble(dynamic value) {
    if (value == null) return 0.0;
    if (value is double) return value;
    if (value is int) return value.toDouble();
    if (value is String) return double.tryParse(value) ?? 0.0;
    return 0.0;
  }
}
