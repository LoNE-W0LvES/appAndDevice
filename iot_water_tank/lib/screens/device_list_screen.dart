import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/auth_provider.dart';
import '../providers/device_provider.dart';
import '../services/offline_mode_service.dart';
import '../widgets/loading_widget.dart';
import '../widgets/error_widget.dart';
import '../widgets/device_card.dart';
import '../widgets/device_sidebar.dart';
import 'water_tank_control_screen.dart';
import 'add_device_screen.dart';
import 'login_screen.dart';
import '../config/app_config.dart';

/// Device list screen with filtering and pull-to-refresh
class DeviceListScreen extends StatefulWidget {
  const DeviceListScreen({Key? key}) : super(key: key);

  @override
  State<DeviceListScreen> createState() => _DeviceListScreenState();
}

class _DeviceListScreenState extends State<DeviceListScreen> {
  final _offlineModeService = OfflineModeService();
  bool _isOfflineMode = false;

  @override
  void initState() {
    super.initState();
    // Fetch devices when screen loads
    WidgetsBinding.instance.addPostFrameCallback((_) {
      _checkOfflineMode();
      context.read<DeviceProvider>().fetchDevices();
    });
  }

  Future<void> _checkOfflineMode() async {
    await _offlineModeService.initialize();
    final isOffline = await _offlineModeService.isOfflineModeEnabled();
    setState(() {
      _isOfflineMode = isOffline;
    });
  }

  Future<void> _handleRefresh() async {
    await context.read<DeviceProvider>().refreshDevices();
  }

  void _showProjectFilter() {
    final deviceProvider = context.read<DeviceProvider>();
    final projects = deviceProvider.projects;

    if (projects.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text('No projects available for filtering'),
        ),
      );
      return;
    }

    showModalBottomSheet(
      context: context,
      builder: (context) {
        return Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Padding(
              padding: const EdgeInsets.all(16),
              child: Row(
                children: [
                  Text(
                    'Filter by Project',
                    style: Theme.of(context).textTheme.titleLarge,
                  ),
                  const Spacer(),
                  IconButton(
                    icon: const Icon(Icons.close),
                    onPressed: () => Navigator.pop(context),
                  ),
                ],
              ),
            ),
            const Divider(height: 1),
            ListTile(
              leading: const Icon(Icons.all_inclusive),
              title: const Text('All Projects'),
              trailing: deviceProvider.selectedProjectId == null
                  ? const Icon(Icons.check)
                  : null,
              onTap: () {
                deviceProvider.clearProjectFilter();
                Navigator.pop(context);
              },
            ),
            ...projects.entries.map((entry) {
              final isSelected = deviceProvider.selectedProjectId == entry.key;
              return ListTile(
                leading: const Icon(Icons.folder_outlined),
                title: Text(entry.value),
                trailing: isSelected ? const Icon(Icons.check) : null,
                onTap: () {
                  deviceProvider.setProjectFilter(entry.key);
                  Navigator.pop(context);
                },
              );
            }),
            const SizedBox(height: 16),
          ],
        );
      },
    );
  }

  void _handleLogout() async {
    final title = _isOfflineMode ? 'Exit Offline Mode' : 'Logout';
    final content = _isOfflineMode
        ? 'Exit offline mode and return to login?'
        : 'Are you sure you want to logout?';

    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: Text(title),
        content: Text(content),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, true),
            child: Text(_isOfflineMode ? 'Exit' : 'Logout'),
          ),
        ],
      ),
    );

    if (confirmed == true && mounted) {
      if (_isOfflineMode) {
        // Exit offline mode
        await _offlineModeService.disableOfflineMode();
        // Navigate to login screen
        if (mounted) {
          Navigator.of(context).pushAndRemoveUntil(
            MaterialPageRoute(builder: (context) => const LoginScreen()),
            (route) => false,
          );
        }
      } else {
        // Normal logout
        await context.read<AuthProvider>().signOut();
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Row(
          children: [
            const Text('Devices'),
            if (_isOfflineMode) ...[
              const SizedBox(width: 8),
              Chip(
                avatar: const Icon(Icons.wifi_off_rounded, size: 16),
                label: const Text('Offline', style: TextStyle(fontSize: 12)),
                visualDensity: VisualDensity.compact,
                backgroundColor: Theme.of(context).colorScheme.secondaryContainer,
              ),
            ],
          ],
        ),
        leading: Builder(
          builder: (context) => IconButton(
            icon: const Icon(Icons.menu),
            onPressed: () => Scaffold.of(context).openDrawer(),
            tooltip: 'Device Management',
          ),
        ),
        actions: [
          // Only show filter button if no project is hardcoded in config
          if (AppConfig.projectId == null)
            Consumer<DeviceProvider>(
              builder: (context, deviceProvider, child) {
                return IconButton(
                  icon: Badge(
                    isLabelVisible: deviceProvider.selectedProjectId != null,
                    child: const Icon(Icons.filter_list),
                  ),
                  onPressed: _showProjectFilter,
                );
              },
            ),
          IconButton(
            icon: const Icon(Icons.logout),
            onPressed: _handleLogout,
          ),
        ],
      ),
      drawer: const DeviceSidebar(),
      body: Consumer<DeviceProvider>(
        builder: (context, deviceProvider, child) {
          // Loading state
          if (deviceProvider.isLoading && deviceProvider.devices.isEmpty) {
            return const LoadingWidget(message: 'Loading devices...');
          }

          // Error state
          if (deviceProvider.error != null && deviceProvider.devices.isEmpty) {
            return ErrorDisplayWidget(
              message: deviceProvider.error!,
              onRetry: () => deviceProvider.fetchDevices(),
            );
          }

          // Empty state
          if (deviceProvider.devices.isEmpty) {
            return Center(
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  Icon(
                    Icons.devices_other,
                    size: 64,
                    color: Theme.of(context).colorScheme.outline,
                  ),
                  const SizedBox(height: 16),
                  Text(
                    'No devices found',
                    style: Theme.of(context).textTheme.titleMedium,
                  ),
                  const SizedBox(height: 8),
                  Text(
                    deviceProvider.selectedProjectId != null
                        ? 'Try changing the project filter'
                        : 'Pull down to refresh',
                    style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                          color: Theme.of(context).colorScheme.onSurfaceVariant,
                        ),
                  ),
                  const SizedBox(height: 24),
                  FilledButton.icon(
                    onPressed: _handleRefresh,
                    icon: const Icon(Icons.refresh),
                    label: const Text('Refresh'),
                  ),
                ],
              ),
            );
          }

          // Device list
          return RefreshIndicator(
            onRefresh: _handleRefresh,
            child: Column(
              children: [
                // Filter info
                if (deviceProvider.selectedProjectId != null)
                  Container(
                    padding: const EdgeInsets.all(12),
                    color: Theme.of(context).colorScheme.secondaryContainer,
                    child: Row(
                      children: [
                        Icon(
                          Icons.filter_list,
                          size: 16,
                          color: Theme.of(context).colorScheme.onSecondaryContainer,
                        ),
                        const SizedBox(width: 8),
                        Expanded(
                          child: Text(
                            'Filtered by: ${deviceProvider.projects[deviceProvider.selectedProjectId]}',
                            style: TextStyle(
                              color:
                                  Theme.of(context).colorScheme.onSecondaryContainer,
                            ),
                          ),
                        ),
                        TextButton(
                          onPressed: deviceProvider.clearProjectFilter,
                          child: const Text('Clear'),
                        ),
                      ],
                    ),
                  ),

                // Device list
                Expanded(
                  child: ListView.builder(
                    itemCount: deviceProvider.devices.length,
                    padding: const EdgeInsets.symmetric(vertical: 8),
                    itemBuilder: (context, index) {
                      final device = deviceProvider.devices[index];
                      return DeviceCard(
                        device: device,
                        onTap: () {
                          // Set the selected device before navigating
                          deviceProvider.setSelectedDevice(device);
                          Navigator.push(
                            context,
                            MaterialPageRoute(
                              builder: (context) => const WaterTankControlScreen(),
                            ),
                          );
                        },
                      );
                    },
                  ),
                ),
              ],
            ),
          );
        },
      ),
      floatingActionButton: FloatingActionButton.extended(
        onPressed: () {
          Navigator.push(
            context,
            MaterialPageRoute(
              builder: (context) => const AddDeviceScreen(),
            ),
          );
        },
        icon: const Icon(Icons.add),
        label: const Text('Add Device'),
      ),
    );
  }
}
