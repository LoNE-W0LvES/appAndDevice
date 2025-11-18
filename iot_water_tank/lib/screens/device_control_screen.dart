import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/device_provider.dart';
import '../widgets/loading_widget.dart';
import '../widgets/error_widget.dart';
import '../widgets/control_widgets.dart';

/// Device control screen with interactive controls and telemetry display
class DeviceControlScreen extends StatelessWidget {
  const DeviceControlScreen({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Device Control'),
        actions: [
          IconButton(
            icon: const Icon(Icons.refresh),
            onPressed: () {
              final deviceProvider = context.read<DeviceProvider>();
              if (deviceProvider.selectedDevice != null) {
                deviceProvider.selectDevice(deviceProvider.selectedDevice!.id);
              }
            },
          ),
        ],
      ),
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
            return const Center(
              child: Text('No device selected'),
            );
          }

          final userControls = device.getUserControls();
          final systemControls = device.getSystemControls();
          final telemetry = device.telemetryData;

          return RefreshIndicator(
            onRefresh: () async {
              await deviceProvider.selectDevice(device.id);
            },
            child: ListView(
              padding: const EdgeInsets.symmetric(vertical: 16),
              children: [
                // Device Info Card
                Card(
                  margin: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
                  child: Padding(
                    padding: const EdgeInsets.all(16),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Row(
                          children: [
                            Expanded(
                              child: Column(
                                crossAxisAlignment: CrossAxisAlignment.start,
                                children: [
                                  Text(
                                    device.name,
                                    style: Theme.of(context)
                                        .textTheme
                                        .headlineSmall
                                        ?.copyWith(
                                          fontWeight: FontWeight.bold,
                                        ),
                                  ),
                                  const SizedBox(height: 4),
                                  Text(
                                    device.deviceId,
                                    style: Theme.of(context)
                                        .textTheme
                                        .bodyMedium
                                        ?.copyWith(
                                          color: Theme.of(context)
                                              .colorScheme
                                              .onSurfaceVariant,
                                        ),
                                  ),
                                ],
                              ),
                            ),
                            Container(
                              padding: const EdgeInsets.symmetric(
                                horizontal: 12,
                                vertical: 6,
                              ),
                              decoration: BoxDecoration(
                                color: device.isActive
                                    ? Colors.green
                                    : Colors.grey,
                                borderRadius: BorderRadius.circular(16),
                              ),
                              child: Text(
                                device.isActive ? 'Active' : 'Inactive',
                                style: const TextStyle(
                                  color: Colors.white,
                                  fontWeight: FontWeight.bold,
                                  fontSize: 12,
                                ),
                              ),
                            ),
                          ],
                        ),
                        const SizedBox(height: 12),
                        const Divider(),
                        const SizedBox(height: 8),
                        Row(
                          children: [
                            Icon(
                              Icons.access_time,
                              size: 16,
                              color: Theme.of(context).colorScheme.onSurfaceVariant,
                            ),
                            const SizedBox(width: 4),
                            Text(
                              'Last seen: ${device.getStatusText()}',
                              style: Theme.of(context).textTheme.bodySmall,
                            ),
                          ],
                        ),
                        if (device.projectName != null) ...[
                          const SizedBox(height: 8),
                          Row(
                            children: [
                              Icon(
                                Icons.folder_outlined,
                                size: 16,
                                color:
                                    Theme.of(context).colorScheme.onSurfaceVariant,
                              ),
                              const SizedBox(width: 4),
                              Text(
                                'Project: ${device.projectName}',
                                style: Theme.of(context).textTheme.bodySmall,
                              ),
                            ],
                          ),
                        ],
                      ],
                    ),
                  ),
                ),

                // User Controls Section
                if (userControls.isNotEmpty) ...[
                  Padding(
                    padding: const EdgeInsets.fromLTRB(16, 24, 16, 8),
                    child: Text(
                      'CONTROLS',
                      style: Theme.of(context).textTheme.titleSmall?.copyWith(
                            color: Theme.of(context).colorScheme.primary,
                            fontWeight: FontWeight.bold,
                          ),
                    ),
                  ),
                  ...userControls.entries.map((entry) {
                    final control = entry.value;
                    if (control.type == 'boolean') {
                      return BooleanControlWidget(
                        control: control,
                        onChanged: (value) async {
                          final success = await deviceProvider.updateControl(
                            control.key,
                            value,
                            'boolean',
                          );
                          if (!success && context.mounted) {
                            ScaffoldMessenger.of(context).showSnackBar(
                              SnackBar(
                                content:
                                    Text(deviceProvider.error ?? 'Update failed'),
                                backgroundColor: Theme.of(context).colorScheme.error,
                              ),
                            );
                          }
                        },
                      );
                    } else if (control.type == 'number') {
                      return NumberControlWidget(
                        control: control,
                        min: 0,
                        max: 100,
                        onChanged: (value) async {
                          final success = await deviceProvider.updateControl(
                            control.key,
                            value,
                            'number',
                          );
                          if (!success && context.mounted) {
                            ScaffoldMessenger.of(context).showSnackBar(
                              SnackBar(
                                content:
                                    Text(deviceProvider.error ?? 'Update failed'),
                                backgroundColor: Theme.of(context).colorScheme.error,
                              ),
                            );
                          }
                        },
                      );
                    }
                    return const SizedBox.shrink();
                  }),
                ],

                // System Controls Section
                if (systemControls.isNotEmpty) ...[
                  Padding(
                    padding: const EdgeInsets.fromLTRB(16, 24, 16, 8),
                    child: Text(
                      'SYSTEM CONTROLS',
                      style: Theme.of(context).textTheme.titleSmall?.copyWith(
                            color: Theme.of(context).colorScheme.tertiary,
                            fontWeight: FontWeight.bold,
                          ),
                    ),
                  ),
                  ...systemControls.entries.map((entry) {
                    final control = entry.value;
                    if (control.type == 'boolean') {
                      return BooleanControlWidget(
                        control: control,
                        isSystem: true,
                        onChanged: (value) async {
                          final success = await deviceProvider.updateControl(
                            control.key,
                            value,
                            'boolean',
                          );
                          if (!success && context.mounted) {
                            ScaffoldMessenger.of(context).showSnackBar(
                              SnackBar(
                                content:
                                    Text(deviceProvider.error ?? 'Update failed'),
                                backgroundColor: Theme.of(context).colorScheme.error,
                              ),
                            );
                          }
                        },
                      );
                    }
                    return const SizedBox.shrink();
                  }),
                ],

                // Telemetry Data Section
                if (telemetry.isNotEmpty) ...[
                  Padding(
                    padding: const EdgeInsets.fromLTRB(16, 24, 16, 8),
                    child: Text(
                      'TELEMETRY DATA',
                      style: Theme.of(context).textTheme.titleSmall?.copyWith(
                            color: Theme.of(context).colorScheme.secondary,
                            fontWeight: FontWeight.bold,
                          ),
                    ),
                  ),
                  Card(
                    margin: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
                    child: Padding(
                      padding: const EdgeInsets.all(16),
                      child: Column(
                        children: telemetry.entries.map((entry) {
                          final data = entry.value;
                          return Padding(
                            padding: const EdgeInsets.symmetric(vertical: 8),
                            child: Row(
                              mainAxisAlignment: MainAxisAlignment.spaceBetween,
                              children: [
                                Text(
                                  _formatKey(data.key),
                                  style: Theme.of(context).textTheme.bodyMedium,
                                ),
                                Container(
                                  padding: const EdgeInsets.symmetric(
                                    horizontal: 12,
                                    vertical: 6,
                                  ),
                                  decoration: BoxDecoration(
                                    color: Theme.of(context)
                                        .colorScheme
                                        .secondaryContainer,
                                    borderRadius: BorderRadius.circular(8),
                                  ),
                                  child: Text(
                                    data.displayValue,
                                    style: Theme.of(context)
                                        .textTheme
                                        .titleSmall
                                        ?.copyWith(
                                          color: Theme.of(context)
                                              .colorScheme
                                              .onSecondaryContainer,
                                          fontWeight: FontWeight.bold,
                                        ),
                                  ),
                                ),
                              ],
                            ),
                          );
                        }).toList(),
                      ),
                    ),
                  ),
                ],

                // Empty state
                if (userControls.isEmpty &&
                    systemControls.isEmpty &&
                    telemetry.isEmpty) ...[
                  const SizedBox(height: 48),
                  Center(
                    child: Column(
                      children: [
                        Icon(
                          Icons.info_outline,
                          size: 64,
                          color: Theme.of(context).colorScheme.outline,
                        ),
                        const SizedBox(height: 16),
                        Text(
                          'No control or telemetry data available',
                          style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                                color:
                                    Theme.of(context).colorScheme.onSurfaceVariant,
                              ),
                        ),
                      ],
                    ),
                  ),
                ],
              ],
            ),
          );
        },
      ),
    );
  }

  String _formatKey(String key) {
    return key
        .replaceAllMapped(
          RegExp(r'([A-Z])'),
          (match) => ' ${match.group(0)}',
        )
        .replaceAll('_', ' ')
        .trim()
        .split(' ')
        .map((word) =>
            word.isNotEmpty ? '${word[0].toUpperCase()}${word.substring(1)}' : '')
        .join(' ');
  }
}
