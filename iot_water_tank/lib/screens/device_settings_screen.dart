import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:provider/provider.dart';
import '../models/device.dart';
import '../providers/device_provider.dart';
import '../services/device_service.dart';
import '../services/offline_mode_service.dart';
import '../utils/api_exception.dart';
import 'water_tank_control_screen.dart';
import 'device_config_edit_screen.dart';
import 'wifi_setup_screen.dart';

/// Device Settings Screen for viewing device info and removing device
class DeviceSettingsScreen extends StatefulWidget {
  final Device device;

  const DeviceSettingsScreen({
    Key? key,
    required this.device,
  }) : super(key: key);

  @override
  State<DeviceSettingsScreen> createState() => _DeviceSettingsScreenState();
}

class _DeviceSettingsScreenState extends State<DeviceSettingsScreen> {
  final _deviceService = DeviceService();
  final _offlineModeService = OfflineModeService();
  final _ipController = TextEditingController();
  bool _isRemoving = false;
  bool _isOfflineMode = false;
  bool _isEditingIp = false;

  // IP address validation regex
  static final _ipRegex = RegExp(
    r'^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$',
  );

  @override
  void initState() {
    super.initState();
    _deviceService.initialize();
    _checkOfflineMode();
    _ipController.text = widget.device.localIp ?? '';
  }

  @override
  void dispose() {
    _ipController.dispose();
    super.dispose();
  }

  Future<void> _checkOfflineMode() async {
    await _offlineModeService.initialize();
    final isOffline = await _offlineModeService.isOfflineModeEnabled();
    setState(() {
      _isOfflineMode = isOffline;
    });
  }

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

  void _copyToClipboard(String text, String label) {
    Clipboard.setData(ClipboardData(text: text));
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text('$label copied to clipboard'),
        duration: const Duration(seconds: 2),
      ),
    );
  }

  Future<void> _saveIpAddress() async {
    final newIp = _ipController.text.trim();

    // Validate IP if not empty
    if (newIp.isNotEmpty && !_ipRegex.hasMatch(newIp)) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text('Invalid IP address format'),
          backgroundColor: Colors.red,
        ),
      );
      return;
    }

    try {
      await _offlineModeService.updateDeviceIp(widget.device.deviceId, newIp);

      setState(() {
        _isEditingIp = false;
      });

      // Refresh device list to update the IP
      await context.read<DeviceProvider>().refreshDevices();

      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text('Device IP address updated'),
            backgroundColor: Colors.green,
          ),
        );
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text('Failed to update IP address'),
            backgroundColor: Colors.red,
          ),
        );
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    final colorScheme = Theme.of(context).colorScheme;
    final textTheme = Theme.of(context).textTheme;

    return Scaffold(
      appBar: AppBar(
        title: Text('${widget.device.name} Settings'),
        actions: [
          IconButton(
            icon: const Icon(Icons.settings_remote),
            tooltip: 'Device Controls',
            onPressed: () {
              // Set device directly (we already have the full object)
              context.read<DeviceProvider>().setSelectedDevice(widget.device);
              Navigator.push(
                context,
                MaterialPageRoute(
                  builder: (context) => const WaterTankControlScreen(),
                ),
              );
            },
          ),
        ],
      ),
      body: Stack(
        children: [
          ListView(
            padding: const EdgeInsets.all(16),
            children: [
              // Device Information Section
              Text(
                'DEVICE INFORMATION',
                style: textTheme.titleSmall?.copyWith(
                  color: colorScheme.primary,
                  fontWeight: FontWeight.bold,
                ),
              ),
              const SizedBox(height: 12),

              Card(
                child: Padding(
                  padding: const EdgeInsets.all(16),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      // Device Name
                      _buildInfoRow(
                        context,
                        icon: Icons.devices,
                        label: 'Device Name',
                        value: widget.device.name,
                      ),
                      const Divider(height: 24),

                      // Device ID (Copyable)
                      _buildInfoRow(
                        context,
                        icon: Icons.fingerprint,
                        label: 'Device ID',
                        value: widget.device.deviceId,
                        copyable: true,
                        onCopy: () => _copyToClipboard(
                          widget.device.deviceId,
                          'Device ID',
                        ),
                      ),
                      const Divider(height: 24),

                      // Local IP Address (Editable in offline mode)
                      if (_isOfflineMode) ...[
                        Row(
                          children: [
                            Icon(
                              Icons.router,
                              size: 16,
                              color: colorScheme.onSurfaceVariant,
                            ),
                            const SizedBox(width: 12),
                            Expanded(
                              child: Column(
                                crossAxisAlignment: CrossAxisAlignment.start,
                                children: [
                                  Text(
                                    'Local IP Address',
                                    style: textTheme.bodySmall?.copyWith(
                                      color: colorScheme.onSurfaceVariant,
                                    ),
                                  ),
                                  const SizedBox(height: 4),
                                  if (_isEditingIp)
                                    Row(
                                      children: [
                                        Expanded(
                                          child: TextField(
                                            controller: _ipController,
                                            keyboardType: TextInputType.number,
                                            decoration: InputDecoration(
                                              hintText: '192.168.1.100',
                                              isDense: true,
                                              contentPadding: const EdgeInsets.symmetric(
                                                horizontal: 8,
                                                vertical: 8,
                                              ),
                                              border: OutlineInputBorder(
                                                borderRadius: BorderRadius.circular(4),
                                              ),
                                            ),
                                          ),
                                        ),
                                        const SizedBox(width: 8),
                                        IconButton(
                                          icon: const Icon(Icons.check, size: 20),
                                          onPressed: _saveIpAddress,
                                          color: Colors.green,
                                        ),
                                        IconButton(
                                          icon: const Icon(Icons.close, size: 20),
                                          onPressed: () {
                                            setState(() {
                                              _isEditingIp = false;
                                              _ipController.text = widget.device.localIp ?? '';
                                            });
                                          },
                                          color: Colors.red,
                                        ),
                                      ],
                                    )
                                  else
                                    Row(
                                      children: [
                                        Expanded(
                                          child: Text(
                                            widget.device.localIp?.isNotEmpty == true
                                                ? widget.device.localIp!
                                                : 'Not set (configure after WiFi setup)',
                                            style: textTheme.bodyMedium?.copyWith(
                                              fontWeight: FontWeight.w500,
                                              color: widget.device.localIp?.isNotEmpty == true
                                                  ? null
                                                  : colorScheme.onSurfaceVariant.withOpacity(0.6),
                                            ),
                                          ),
                                        ),
                                        IconButton(
                                          icon: const Icon(Icons.edit, size: 20),
                                          onPressed: () {
                                            setState(() {
                                              _isEditingIp = true;
                                            });
                                          },
                                        ),
                                      ],
                                    ),
                                ],
                              ),
                            ),
                          ],
                        ),
                        const Divider(height: 24),
                      ],

                      // Project Name
                      if (widget.device.projectName != null) ...[
                        _buildInfoRow(
                          context,
                          icon: Icons.folder_outlined,
                          label: 'Project',
                          value: widget.device.projectName!,
                        ),
                        const Divider(height: 24),
                      ],

                      // Status
                      Row(
                        children: [
                          Icon(
                            Icons.circle,
                            size: 16,
                            color: colorScheme.onSurfaceVariant,
                          ),
                          const SizedBox(width: 12),
                          Expanded(
                            child: Column(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                Text(
                                  'Status',
                                  style: textTheme.bodySmall?.copyWith(
                                    color: colorScheme.onSurfaceVariant,
                                  ),
                                ),
                                const SizedBox(height: 4),
                                Container(
                                  padding: const EdgeInsets.symmetric(
                                    horizontal: 12,
                                    vertical: 6,
                                  ),
                                  decoration: BoxDecoration(
                                    color: widget.device.isActive
                                        ? Colors.green.withOpacity(0.2)
                                        : Colors.grey.withOpacity(0.2),
                                    borderRadius: BorderRadius.circular(16),
                                  ),
                                  child: Text(
                                    widget.device.isActive
                                        ? 'Active'
                                        : 'Inactive',
                                    style: TextStyle(
                                      color: widget.device.isActive
                                          ? Colors.green
                                          : Colors.grey,
                                      fontWeight: FontWeight.bold,
                                      fontSize: 12,
                                    ),
                                  ),
                                ),
                              ],
                            ),
                          ),
                        ],
                      ),
                      const Divider(height: 24),

                      // Last Seen
                      _buildInfoRow(
                        context,
                        icon: Icons.access_time,
                        label: 'Last Seen',
                        value: widget.device.getStatusText(),
                      ),
                    ],
                  ),
                ),
              ),

              const SizedBox(height: 32),

              // Configuration Section
              Text(
                'CONFIGURATION',
                style: textTheme.titleSmall?.copyWith(
                  color: colorScheme.primary,
                  fontWeight: FontWeight.bold,
                ),
              ),
              const SizedBox(height: 12),

              Card(
                child: Column(
                  children: [
                    ListTile(
                      leading: Icon(
                        Icons.settings,
                        color: colorScheme.primary,
                      ),
                      title: const Text('Device Configuration'),
                      subtitle: Text(
                        widget.device.deviceConfig.isEmpty
                            ? 'No configuration available'
                            : '${widget.device.deviceConfig.length} parameter(s)',
                      ),
                      trailing: widget.device.deviceConfig.isNotEmpty
                          ? const Icon(Icons.arrow_forward_ios, size: 16)
                          : null,
                      onTap: widget.device.deviceConfig.isEmpty
                          ? null
                          : () {
                              Navigator.push(
                                context,
                                MaterialPageRoute(
                                  builder: (context) => DeviceConfigEditScreen(
                                    device: widget.device,
                                  ),
                                ),
                              );
                            },
                    ),
                    const Divider(height: 1),
                    ListTile(
                      leading: Icon(
                        Icons.build,
                        color: colorScheme.primary,
                      ),
                      title: const Text('Connection Configuration'),
                      subtitle: const Text('WiFi and dashboard credentials'),
                      trailing: const Icon(Icons.arrow_forward_ios, size: 16),
                      onTap: () {
                        Navigator.push(
                          context,
                          MaterialPageRoute(
                            builder: (context) => WiFiSetupScreen(
                              deviceId: widget.device.deviceId,
                              deviceName: widget.device.name,
                            ),
                          ),
                        );
                      },
                    ),
                  ],
                ),
              ),

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
                          Icon(
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
            ],
          ),

          // Full screen loading overlay
          if (_isRemoving)
            Container(
              color: Colors.black.withOpacity(0.5),
              child: const Center(
                child: Card(
                  child: Padding(
                    padding: EdgeInsets.all(24),
                    child: Column(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        CircularProgressIndicator(),
                        SizedBox(height: 16),
                        Text('Removing device...'),
                      ],
                    ),
                  ),
                ),
              ),
            ),
        ],
      ),
    );
  }

  Widget _buildInfoRow(
    BuildContext context, {
    required IconData icon,
    required String label,
    required String value,
    bool copyable = false,
    VoidCallback? onCopy,
  }) {
    final colorScheme = Theme.of(context).colorScheme;
    final textTheme = Theme.of(context).textTheme;

    return Row(
      children: [
        Icon(
          icon,
          size: 16,
          color: colorScheme.onSurfaceVariant,
        ),
        const SizedBox(width: 12),
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                label,
                style: textTheme.bodySmall?.copyWith(
                  color: colorScheme.onSurfaceVariant,
                ),
              ),
              const SizedBox(height: 4),
              Text(
                value,
                style: textTheme.bodyMedium?.copyWith(
                  fontWeight: FontWeight.w500,
                ),
              ),
            ],
          ),
        ),
        if (copyable && onCopy != null)
          IconButton(
            icon: const Icon(Icons.copy, size: 18),
            onPressed: onCopy,
            tooltip: 'Copy',
          ),
      ],
    );
  }
}
