import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../models/device.dart';
import '../models/device_config_parameter.dart';
import '../providers/device_provider.dart';
import '../services/device_service.dart';
import '../services/offline_device_service.dart';
import '../services/offline_mode_service.dart';
import '../utils/api_exception.dart';
import '../config/app_config.dart';

/// Screen for editing device configuration parameters
class DeviceConfigEditScreen extends StatefulWidget {
  final Device device;

  const DeviceConfigEditScreen({
    Key? key,
    required this.device,
  }) : super(key: key);

  @override
  State<DeviceConfigEditScreen> createState() => _DeviceConfigEditScreenState();
}

class _DeviceConfigEditScreenState extends State<DeviceConfigEditScreen> {
  final _formKey = GlobalKey<FormState>();
  final _deviceService = DeviceService();
  final _offlineDeviceService = OfflineDeviceService();
  final _offlineModeService = OfflineModeService();

  // Track original and modified config
  late Map<String, DeviceConfigParameter> _originalConfig;
  late Map<String, DeviceConfigParameter> _modifiedConfig;

  // Track which fields changed
  final Set<String> _changedFields = {};

  bool _isSaving = false;
  bool _isRemoving = false;

  @override
  void initState() {
    super.initState();
    _deviceService.initialize();
    _offlineDeviceService.initialize();
    _offlineModeService.initialize();
    _originalConfig = Map.from(widget.device.deviceConfig);
    _modifiedConfig = Map.from(widget.device.deviceConfig);
  }

  /// Check if a field has changed
  bool _hasChanged(String key) {
    return _changedFields.contains(key);
  }

  /// Mark field as changed and update modified config
  void _updateField(String key, dynamic newValue) {
    setState(() {
      final param = _modifiedConfig[key]!;
      _modifiedConfig[key] = param.withNewValue(newValue);

      // Mark as changed only if different from original
      if (newValue != _originalConfig[key]?.value) {
        _changedFields.add(key);
      } else {
        _changedFields.remove(key);
      }
    });
  }

  /// Save configuration changes
  Future<void> _handleSave() async {
    if (!_formKey.currentState!.validate()) return;
    if (_changedFields.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text('No changes to save'),
          duration: Duration(seconds: 2),
        ),
      );
      return;
    }

    setState(() {
      _isSaving = true;
    });

    try {
      // Prepare only changed parameters with new timestamps
      final changedConfig = <String, DeviceConfigParameter>{};
      for (final key in _changedFields) {
        changedConfig[key] = _modifiedConfig[key]!;
      }

      // Log configuration being saved
      AppConfig.configLog('=== SAVING DEVICE CONFIGURATION ===');
      AppConfig.configLog('Device ID: ${widget.device.id}');
      AppConfig.configLog('Device Name: ${widget.device.name}');
      AppConfig.configLog('Changed Fields: ${_changedFields.length}');
      AppConfig.configLog('');
      for (final entry in changedConfig.entries) {
        AppConfig.configLog('${entry.key}:');
        AppConfig.configLog('  Type: ${entry.value.type}');
        AppConfig.configLog('  Old Value: ${_originalConfig[entry.key]?.value}');
        AppConfig.configLog('  New Value: ${entry.value.value}');
        AppConfig.configLog('  Timestamp: ${entry.value.lastModified}');
        if (entry.value.options != null) {
          AppConfig.configLog('  Options: ${entry.value.options}');
        }
        AppConfig.configLog('');
      }
      AppConfig.configLog('===================================');

      // Check if offline mode is enabled
      final isOffline = await _offlineModeService.isOfflineModeEnabled();

      if (isOffline) {
        // Offline mode: Use OfflineDeviceService with local IP
        final localIp = widget.device.localIp;

        if (localIp == null || localIp.isEmpty) {
          throw ApiException(
            message: 'Device local IP not set. Please set IP in device settings.',
          );
        }

        // Update config via local endpoint
        await _offlineDeviceService.updateDeviceConfig(
          widget.device.deviceId,
          changedConfig,
          localIp: localIp,
        );

        // Fetch updated device data
        final updatedDevice = await _offlineDeviceService.getDevice(
          widget.device.deviceId,
          localIp: localIp,
        );

        if (!mounted) return;

        // Update provider
        context.read<DeviceProvider>().setSelectedDevice(updatedDevice);
      } else {
        // Online mode: Use DeviceService (this will also set config_update = true)
        final updatedDevice = await _deviceService.updateDeviceConfig(
          widget.device.id,
          changedConfig,
        );

        if (!mounted) return;

        // Update provider
        context.read<DeviceProvider>().setSelectedDevice(updatedDevice);
      }

      if (!mounted) return;

      // Refresh device list
      await context.read<DeviceProvider>().refreshDevices();

      // Show success message
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(
            'Configuration updated successfully. ${_changedFields.length} parameter(s) changed.',
          ),
          backgroundColor: Colors.green,
          duration: const Duration(seconds: 3),
        ),
      );

      // Navigate back
      if (mounted) {
        Navigator.pop(context);
      }
    } on ApiException catch (e) {
      setState(() {
        _isSaving = false;
      });

      if (!mounted) return;

      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(e.message),
          backgroundColor: Colors.red,
          duration: const Duration(seconds: 4),
        ),
      );
    } catch (e) {
      setState(() {
        _isSaving = false;
      });

      if (!mounted) return;

      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text('An unexpected error occurred. Please try again.'),
          backgroundColor: Colors.red,
          duration: Duration(seconds: 4),
        ),
      );
    }
  }

  /// Handle device removal
  Future<void> _handleRemoveDevice() async {
    // Show confirmation dialog
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Remove Device?'),
        content: Text(
          'Are you sure you want to remove ${widget.device.name} from your account? '
          'You will need the Device ID to add it again.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, true),
            style: FilledButton.styleFrom(
              backgroundColor: Colors.red,
            ),
            child: const Text('Remove'),
          ),
        ],
      ),
    );

    if (confirmed != true) return;

    setState(() {
      _isRemoving = true;
    });

    try {
      final result = await _deviceService.unclaimDevice(widget.device.deviceId);

      if (!mounted) return;

      // Show success message
      final message = result['message'] ?? 'Device removed successfully';
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(message),
          backgroundColor: Colors.green,
          duration: const Duration(seconds: 3),
        ),
      );

      // Refresh device list
      await context.read<DeviceProvider>().refreshDevices();

      // Navigate back to device list (pop all routes)
      if (mounted) {
        Navigator.of(context).popUntil((route) => route.isFirst);
      }
    } on ApiException catch (e) {
      if (!mounted) return;

      setState(() {
        _isRemoving = false;
      });

      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(e.message),
          backgroundColor: Colors.red,
          duration: const Duration(seconds: 4),
        ),
      );
    } catch (e) {
      if (!mounted) return;

      setState(() {
        _isRemoving = false;
      });

      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text('An unexpected error occurred. Please try again.'),
          backgroundColor: Colors.red,
          duration: Duration(seconds: 4),
        ),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    final colorScheme = Theme.of(context).colorScheme;
    final textTheme = Theme.of(context).textTheme;

    // Get config_update flag status
    final configUpdateFlag = widget.device.controlData['config_update'];
    final isConfigPending = configUpdateFlag?.boolValue ?? false;

    return Scaffold(
      appBar: AppBar(
        title: const Text('Device Configuration'),
        actions: [
          // Save button in AppBar
          if (_changedFields.isNotEmpty)
            TextButton.icon(
              onPressed: _isSaving ? null : _handleSave,
              icon: _isSaving
                  ? const SizedBox(
                      width: 16,
                      height: 16,
                      child: CircularProgressIndicator(strokeWidth: 2),
                    )
                  : const Icon(Icons.save),
              label: Text(_isSaving ? 'Saving...' : 'Save'),
            ),
        ],
      ),
      body: Stack(
        children: [
          Form(
            key: _formKey,
            child: ListView(
              padding: const EdgeInsets.all(16),
              children: [
                // Config Update Status Banner
                if (isConfigPending)
                  Card(
                    color: Colors.orange.shade100,
                    child: Padding(
                      padding: const EdgeInsets.all(12),
                      child: Row(
                        children: [
                          Icon(
                            Icons.pending_actions,
                            color: Colors.orange.shade900,
                          ),
                          const SizedBox(width: 12),
                          Expanded(
                            child: Text(
                              'Configuration update pending. Device will apply changes soon.',
                              style: TextStyle(
                                color: Colors.orange.shade900,
                                fontWeight: FontWeight.w500,
                              ),
                            ),
                          ),
                        ],
                      ),
                    ),
                  ),

                const SizedBox(height: 8),

                // Device Info
                Text(
                  'DEVICE: ${widget.device.name}',
                  style: textTheme.titleSmall?.copyWith(
                    color: colorScheme.primary,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                const SizedBox(height: 16),

                // Configuration Parameters
                if (_modifiedConfig.isEmpty)
                  Center(
                    child: Padding(
                      padding: const EdgeInsets.all(48),
                      child: Column(
                        children: [
                          Icon(
                            Icons.settings,
                            size: 64,
                            color: colorScheme.outline,
                          ),
                          const SizedBox(height: 16),
                          Text(
                            'No configuration parameters available',
                            style: textTheme.bodyMedium?.copyWith(
                              color: colorScheme.onSurfaceVariant,
                            ),
                          ),
                        ],
                      ),
                    ),
                  )
                else
                  ..._modifiedConfig.entries
                      .where((entry) => !entry.value.hidden) // Filter out hidden fields
                      .map((entry) {
                    return _buildConfigField(entry.key, entry.value);
                  }),

                const SizedBox(height: 32),

                // Danger Zone Section
                Text(
                  'DANGER ZONE',
                  style: textTheme.titleSmall?.copyWith(
                    color: Colors.red,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                const SizedBox(height: 12),

                Card(
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(12),
                    side: BorderSide(
                      color: Colors.red.withOpacity(0.5),
                      width: 1,
                    ),
                  ),
                  child: Padding(
                    padding: const EdgeInsets.all(16),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Row(
                          children: [
                            const Icon(
                              Icons.warning_outlined,
                              color: Colors.red,
                              size: 20,
                            ),
                            const SizedBox(width: 8),
                            Text(
                              'Remove Device',
                              style: textTheme.titleMedium?.copyWith(
                                color: Colors.red,
                                fontWeight: FontWeight.bold,
                              ),
                            ),
                          ],
                        ),
                        const SizedBox(height: 8),
                        Text(
                          'Remove this device from your account. This action cannot be undone.',
                          style: textTheme.bodySmall?.copyWith(
                            color: colorScheme.onSurfaceVariant,
                          ),
                        ),
                        const SizedBox(height: 16),
                        OutlinedButton.icon(
                          onPressed: _isRemoving ? null : _handleRemoveDevice,
                          icon: _isRemoving
                              ? const SizedBox(
                                  width: 16,
                                  height: 16,
                                  child: CircularProgressIndicator(
                                    strokeWidth: 2,
                                  ),
                                )
                              : const Icon(Icons.delete_outline),
                          label: Text(
                            _isRemoving ? 'Removing...' : 'Remove Device',
                          ),
                          style: OutlinedButton.styleFrom(
                            foregroundColor: Colors.red,
                            side: const BorderSide(color: Colors.red),
                            padding: const EdgeInsets.symmetric(
                              horizontal: 24,
                              vertical: 12,
                            ),
                            shape: RoundedRectangleBorder(
                              borderRadius: BorderRadius.circular(8),
                            ),
                          ),
                        ),
                      ],
                    ),
                  ),
                ),

                const SizedBox(height: 80), // Space for FAB
              ],
            ),
          ),

          // Full screen loading overlay
          if (_isSaving || _isRemoving)
            Container(
              color: Colors.black.withOpacity(0.5),
              child: Center(
                child: Card(
                  child: Padding(
                    padding: const EdgeInsets.all(24),
                    child: Column(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        const CircularProgressIndicator(),
                        const SizedBox(height: 16),
                        Text(_isSaving
                          ? 'Saving configuration...'
                          : 'Removing device...'),
                      ],
                    ),
                  ),
                ),
              ),
            ),
        ],
      ),
      floatingActionButton: _changedFields.isNotEmpty
          ? FloatingActionButton.extended(
              onPressed: _isSaving ? null : _handleSave,
              icon: const Icon(Icons.save),
              label: Text('Save ${_changedFields.length} Change(s)'),
            )
          : null,
    );
  }

  Widget _buildConfigField(String key, DeviceConfigParameter param) {
    final hasChanged = _hasChanged(key);
    final colorScheme = Theme.of(context).colorScheme;

    return Card(
      elevation: hasChanged ? 4 : 1,
      color: hasChanged
          ? colorScheme.primaryContainer.withOpacity(0.3)
          : null,
      margin: const EdgeInsets.only(bottom: 12),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Expanded(
                  child: Text(
                    param.label,
                    style: Theme.of(context).textTheme.titleSmall?.copyWith(
                          fontWeight: FontWeight.bold,
                        ),
                  ),
                ),
                if (hasChanged)
                  Container(
                    padding: const EdgeInsets.symmetric(
                      horizontal: 8,
                      vertical: 4,
                    ),
                    decoration: BoxDecoration(
                      color: colorScheme.primary,
                      borderRadius: BorderRadius.circular(12),
                    ),
                    child: Text(
                      'Modified',
                      style: TextStyle(
                        color: colorScheme.onPrimary,
                        fontSize: 11,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                  ),
              ],
            ),
            const SizedBox(height: 12),
            _buildFieldByType(key, param),
            if (param.lastModified != null) ...[
              const SizedBox(height: 8),
              Text(
                'Last modified: ${_formatTimestamp(param.lastModified!)}',
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                      color: colorScheme.onSurfaceVariant,
                      fontStyle: FontStyle.italic,
                    ),
              ),
            ],
          ],
        ),
      ),
    );
  }

  Widget _buildFieldByType(String key, DeviceConfigParameter param) {
    switch (param.type) {
      case 'boolean':
        return SwitchListTile(
          value: param.boolValue,
          onChanged: (value) => _updateField(key, value),
          title: Text(param.boolValue ? 'Enabled' : 'Disabled'),
          contentPadding: EdgeInsets.zero,
        );

      case 'number':
        return TextFormField(
          initialValue: param.numberValue.toString(),
          keyboardType: TextInputType.number,
          decoration: InputDecoration(
            border: const OutlineInputBorder(),
            suffixText: 'number',
          ),
          validator: (value) {
            if (value == null || value.isEmpty) {
              return 'Please enter a value';
            }
            if (double.tryParse(value) == null) {
              return 'Please enter a valid number';
            }
            return null;
          },
          onChanged: (value) {
            final numValue = double.tryParse(value);
            if (numValue != null) {
              _updateField(key, numValue);
            }
          },
        );

      case 'dropdown':
        final options = param.options ?? [];
        final currentValue = param.stringValue;

        if (options.isEmpty) {
          return Text(
            'No options available',
            style: TextStyle(color: Theme.of(context).colorScheme.error),
          );
        }

        return Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            // Segmented buttons for dropdown options
            Row(
              children: options.map((option) {
                final isSelected = option == currentValue;
                return Expanded(
                  child: Padding(
                    padding: const EdgeInsets.symmetric(horizontal: 4),
                    child: OutlinedButton(
                      onPressed: () => _updateField(key, option),
                      style: OutlinedButton.styleFrom(
                        backgroundColor: isSelected
                            ? Theme.of(context).colorScheme.primaryContainer
                            : null,
                        foregroundColor: isSelected
                            ? Theme.of(context).colorScheme.onPrimaryContainer
                            : Theme.of(context).colorScheme.onSurface,
                        side: BorderSide(
                          color: isSelected
                              ? Theme.of(context).colorScheme.primary
                              : Theme.of(context).colorScheme.outline,
                          width: isSelected ? 2 : 1,
                        ),
                        padding: const EdgeInsets.symmetric(vertical: 12),
                      ),
                      child: Text(
                        option,
                        style: TextStyle(
                          fontWeight: isSelected ? FontWeight.bold : FontWeight.normal,
                        ),
                      ),
                    ),
                  ),
                );
              }).toList(),
            ),
          ],
        );

      case 'string':
      default:
        return TextFormField(
          initialValue: param.stringValue,
          decoration: const InputDecoration(
            border: OutlineInputBorder(),
          ),
          validator: (value) {
            if (value == null || value.isEmpty) {
              return 'Please enter a value';
            }
            return null;
          },
          onChanged: (value) => _updateField(key, value),
        );
    }
  }

  String _formatTimestamp(int timestamp) {
    final date = DateTime.fromMillisecondsSinceEpoch(timestamp);
    final now = DateTime.now();
    final difference = now.difference(date);

    if (difference.inSeconds < 60) {
      return '${difference.inSeconds}s ago';
    } else if (difference.inMinutes < 60) {
      return '${difference.inMinutes}m ago';
    } else if (difference.inHours < 24) {
      return '${difference.inHours}h ago';
    } else if (difference.inDays < 7) {
      return '${difference.inDays}d ago';
    } else {
      return '${date.day}/${date.month}/${date.year}';
    }
  }
}
