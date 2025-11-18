import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/device_provider.dart';
import '../services/device_service.dart';
import '../services/offline_mode_service.dart';
import '../utils/api_exception.dart';

/// Screen for adding a new device by claiming it with Device ID
class AddDeviceScreen extends StatefulWidget {
  const AddDeviceScreen({Key? key}) : super(key: key);

  @override
  State<AddDeviceScreen> createState() => _AddDeviceScreenState();
}

class _AddDeviceScreenState extends State<AddDeviceScreen> {
  final _formKey = GlobalKey<FormState>();
  final _deviceIdController = TextEditingController();
  final _deviceNameController = TextEditingController();
  final _localIpController = TextEditingController();
  final _deviceService = DeviceService();
  final _offlineModeService = OfflineModeService();

  bool _isLoading = false;
  String? _errorMessage;
  bool _isValid = false;
  bool _isOfflineMode = false;

  // Device ID format validation regex
  static final _deviceIdRegex = RegExp(r'^[\w-]+-DEV-\d{2}-[a-f0-9]{24}$');
  // IP address validation regex
  static final _ipRegex = RegExp(
    r'^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$',
  );

  @override
  void initState() {
    super.initState();
    _deviceService.initialize();
    _offlineModeService.initialize();
    _deviceIdController.addListener(_validateDeviceId);
    _checkOfflineMode();
  }

  @override
  void dispose() {
    _deviceIdController.dispose();
    _deviceNameController.dispose();
    _localIpController.dispose();
    super.dispose();
  }

  Future<void> _checkOfflineMode() async {
    final isOffline = await _offlineModeService.isOfflineModeEnabled();
    setState(() {
      _isOfflineMode = isOffline;
    });
  }

  void _validateDeviceId() {
    final deviceId = _deviceIdController.text.trim();
    setState(() {
      _isValid = _deviceIdRegex.hasMatch(deviceId);
      if (_errorMessage != null && _isValid) {
        _errorMessage = null;
      }
    });
  }

  Future<void> _handleAddDevice() async {
    if (!_formKey.currentState!.validate()) return;
    if (!_isValid) return;

    setState(() {
      _isLoading = true;
      _errorMessage = null;
    });

    try {
      final deviceId = _deviceIdController.text.trim();

      if (_isOfflineMode) {
        // Offline mode: Save device locally with manual IP (optional)
        final deviceName = _deviceNameController.text.trim().isNotEmpty
            ? _deviceNameController.text.trim()
            : deviceId; // Use device ID as name if not provided
        final localIp = _localIpController.text.trim(); // Can be empty, set later in settings

        await _offlineModeService.addOfflineDevice(
          deviceId: deviceId,
          deviceName: deviceName,
          localIp: localIp.isEmpty ? '' : localIp, // Store empty string if not provided
        );

        if (!mounted) return;

        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text('Device added to offline mode successfully'),
            backgroundColor: Colors.green,
            duration: Duration(seconds: 3),
          ),
        );
      } else {
        // Online mode: Claim device from server
        final result = await _deviceService.claimDevice(deviceId);

        if (!mounted) return;

        final message = result['message'] ?? 'Device added successfully';
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(message),
            backgroundColor: Colors.green,
            duration: const Duration(seconds: 3),
          ),
        );

        // Refresh device list
        await context.read<DeviceProvider>().refreshDevices();
      }

      // Navigate back
      if (mounted) {
        Navigator.pop(context);
      }
    } on ApiException catch (e) {
      setState(() {
        _errorMessage = e.message;
        _isLoading = false;
      });
    } catch (e) {
      setState(() {
        _errorMessage = 'An unexpected error occurred. Please try again.';
        _isLoading = false;
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    final colorScheme = Theme.of(context).colorScheme;
    final textTheme = Theme.of(context).textTheme;

    return Scaffold(
      appBar: AppBar(
        title: const Text('Add My Device'),
      ),
      body: SafeArea(
        child: SingleChildScrollView(
          padding: const EdgeInsets.all(16),
          child: Form(
            key: _formKey,
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                const SizedBox(height: 24),

                // Info Card
                Card(
                  child: Padding(
                    padding: const EdgeInsets.all(24),
                    child: Column(
                      children: [
                        // Icon
                        Icon(
                          Icons.qr_code_2,
                          size: 80,
                          color: colorScheme.primary,
                        ),
                        const SizedBox(height: 24),

                        // Title
                        Text(
                          _isOfflineMode ? 'Add Offline Device' : 'Claim Your Device',
                          style: textTheme.headlineSmall?.copyWith(
                            fontWeight: FontWeight.bold,
                          ),
                          textAlign: TextAlign.center,
                        ),
                        const SizedBox(height: 8),

                        // Subtitle
                        Text(
                          _isOfflineMode
                              ? 'Enter Device ID and manual IP address'
                              : 'Enter the Device ID from your device',
                          style: textTheme.bodyMedium?.copyWith(
                            color: colorScheme.onSurfaceVariant,
                          ),
                          textAlign: TextAlign.center,
                        ),

                        // Offline mode indicator
                        if (_isOfflineMode) ...[
                          const SizedBox(height: 8),
                          Chip(
                            avatar: const Icon(Icons.wifi_off_rounded, size: 16),
                            label: const Text('Offline Mode'),
                            backgroundColor: colorScheme.secondaryContainer,
                          ),
                        ],
                        const SizedBox(height: 32),

                        // Device ID TextField
                        TextFormField(
                          controller: _deviceIdController,
                          enabled: !_isLoading,
                          textInputAction: TextInputAction.done,
                          onFieldSubmitted: (_) => _handleAddDevice(),
                          decoration: InputDecoration(
                            labelText: 'Device ID',
                            hintText: 'wt001-DEV-01-xxxxxxxx',
                            helperText: 'You can find this ID on your device or from admin',
                            prefixIcon: const Icon(Icons.qr_code_scanner),
                            suffixIcon: _deviceIdController.text.isNotEmpty
                                ? IconButton(
                                    icon: const Icon(Icons.clear),
                                    onPressed: () {
                                      _deviceIdController.clear();
                                      setState(() {
                                        _errorMessage = null;
                                      });
                                    },
                                  )
                                : null,
                            errorText: _errorMessage,
                            errorMaxLines: 3,
                            border: OutlineInputBorder(
                              borderRadius: BorderRadius.circular(12),
                            ),
                          ),
                          validator: (value) {
                            if (value == null || value.trim().isEmpty) {
                              return 'Please enter a Device ID';
                            }
                            if (!_deviceIdRegex.hasMatch(value.trim())) {
                              return 'Invalid Device ID format';
                            }
                            return null;
                          },
                        ),

                        // Offline mode fields
                        if (_isOfflineMode) ...[
                          const SizedBox(height: 16),

                          // Device Name TextField (optional)
                          TextFormField(
                            controller: _deviceNameController,
                            enabled: !_isLoading,
                            textInputAction: TextInputAction.next,
                            decoration: InputDecoration(
                              labelText: 'Device Name (Optional)',
                              hintText: 'My Water Tank',
                              helperText: 'Friendly name for your device',
                              prefixIcon: const Icon(Icons.label_outline),
                              border: OutlineInputBorder(
                                borderRadius: BorderRadius.circular(12),
                              ),
                            ),
                          ),
                          const SizedBox(height: 16),

                          // Local IP TextField (Optional - can be set later in device settings)
                          TextFormField(
                            controller: _localIpController,
                            enabled: !_isLoading,
                            textInputAction: TextInputAction.done,
                            keyboardType: TextInputType.number,
                            onFieldSubmitted: (_) => _handleAddDevice(),
                            decoration: InputDecoration(
                              labelText: 'Device IP Address (Optional)',
                              hintText: '192.168.1.100',
                              helperText: 'Set later in device settings after WiFi configuration',
                              prefixIcon: const Icon(Icons.router),
                              border: OutlineInputBorder(
                                borderRadius: BorderRadius.circular(12),
                              ),
                            ),
                            validator: (value) {
                              // IP is optional, but if provided, must be valid
                              if (value != null && value.trim().isNotEmpty) {
                                if (!_ipRegex.hasMatch(value.trim())) {
                                  return 'Invalid IP address format';
                                }
                              }
                              return null;
                            },
                          ),
                        ],

                        const SizedBox(height: 24),

                        // Add Device Button
                        FilledButton(
                          onPressed: _isLoading || !_isValid
                              ? null
                              : _handleAddDevice,
                          style: FilledButton.styleFrom(
                            padding: const EdgeInsets.symmetric(vertical: 16),
                            shape: RoundedRectangleBorder(
                              borderRadius: BorderRadius.circular(12),
                            ),
                          ),
                          child: _isLoading
                              ? const SizedBox(
                                  height: 20,
                                  width: 20,
                                  child: CircularProgressIndicator(
                                    strokeWidth: 2,
                                  ),
                                )
                              : const Text(
                                  'Add Device',
                                  style: TextStyle(fontSize: 16),
                                ),
                        ),
                      ],
                    ),
                  ),
                ),

                const SizedBox(height: 24),

                // Info Section
                Card(
                  color: colorScheme.surfaceVariant.withOpacity(0.3),
                  child: Padding(
                    padding: const EdgeInsets.all(16),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Row(
                          children: [
                            Icon(
                              Icons.info_outline,
                              size: 20,
                              color: colorScheme.primary,
                            ),
                            const SizedBox(width: 8),
                            Text(
                              'Device ID Format',
                              style: textTheme.titleSmall?.copyWith(
                                fontWeight: FontWeight.bold,
                              ),
                            ),
                          ],
                        ),
                        const SizedBox(height: 12),
                        Text(
                          'Expected format: projectId-DEV-01-xxxxx',
                          style: textTheme.bodySmall?.copyWith(
                            color: colorScheme.onSurfaceVariant,
                          ),
                        ),
                        const SizedBox(height: 4),
                        Text(
                          'Example: wt001-DEV-01-690e6a9d092433c0acfb9178',
                          style: textTheme.bodySmall?.copyWith(
                            color: colorScheme.onSurfaceVariant,
                            fontFamily: 'monospace',
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}
