import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/wifi_setup_provider.dart';
import '../providers/auth_provider.dart';
import '../services/offline_mode_service.dart';
import '../widgets/network_list_tile.dart';

class WiFiSetupScreen extends StatefulWidget {
  final String deviceId;
  final String deviceName;

  const WiFiSetupScreen({
    Key? key,
    required this.deviceId,
    required this.deviceName,
  }) : super(key: key);

  @override
  State<WiFiSetupScreen> createState() => _WiFiSetupScreenState();
}

class _WiFiSetupScreenState extends State<WiFiSetupScreen> {
  final TextEditingController _ssidController = TextEditingController();
  final TextEditingController _passwordController = TextEditingController();
  final TextEditingController _dashboardUsernameController = TextEditingController();
  final TextEditingController _dashboardPasswordController = TextEditingController();
  final OfflineModeService _offlineModeService = OfflineModeService();

  bool _isOfflineMode = false;

  @override
  void initState() {
    super.initState();
    // Reset provider state when screen is opened
    WidgetsBinding.instance.addPostFrameCallback((_) {
      final provider = context.read<WiFiSetupProvider>();
      provider.reset();
      // Check offline mode
      _checkOfflineMode();
      // Automatically check device connection
      _checkConnection();
      // Load saved dashboard credentials (only in online mode)
      _loadSavedCredentials();
    });
  }

  Future<void> _checkOfflineMode() async {
    await _offlineModeService.initialize();
    final isOffline = await _offlineModeService.isOfflineModeEnabled();
    setState(() {
      _isOfflineMode = isOffline;
    });
  }

  Future<void> _loadSavedCredentials() async {
    // Skip loading credentials in offline mode
    if (_isOfflineMode) {
      print('[WiFiSetup] Offline mode: Skipping credential load');
      return;
    }

    final authProvider = context.read<AuthProvider>();
    final username = await authProvider.getDashboardUsername();
    final password = await authProvider.getDashboardPassword();

    print('[WiFiSetup] Loading saved credentials: username=$username, hasPassword=${password != null && password.isNotEmpty}');

    if (username != null && username.isNotEmpty) {
      setState(() {
        _dashboardUsernameController.text = username;
      });
      print('[WiFiSetup] Username autofilled: $username');
    }
    if (password != null && password.isNotEmpty) {
      setState(() {
        _dashboardPasswordController.text = password;
      });
      print('[WiFiSetup] Password autofilled');
    }
  }

  Future<void> _checkConnection() async {
    final provider = context.read<WiFiSetupProvider>();
    await provider.checkDeviceConnection(widget.deviceId);
  }

  Future<void> _scanNetworks() async {
    final provider = context.read<WiFiSetupProvider>();
    final success = await provider.scanWiFiNetworks(widget.deviceId);

    if (success && mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('Found ${provider.availableNetworks.length} networks'),
          backgroundColor: Colors.green,
        ),
      );
    }
  }

  Future<void> _saveCredentials() async {
    final provider = context.read<WiFiSetupProvider>();
    final authProvider = context.read<AuthProvider>();

    // Update provider with text field values
    provider.setSelectedSSID(_ssidController.text);
    provider.setPassword(_passwordController.text);
    provider.setDashboardUsername(_dashboardUsernameController.text);
    provider.setDashboardPassword(_dashboardPasswordController.text);

    final success = await provider.saveWiFiCredentials(widget.deviceId);

    if (success && mounted) {
      // Save dashboard credentials for future autofill
      final dashUsername = _dashboardUsernameController.text.trim();
      final dashPassword = _dashboardPasswordController.text.trim();
      print('[WiFiSetup] Saving credentials: username=$dashUsername, hasPassword=${dashPassword.isNotEmpty}');
      if (dashUsername.isNotEmpty && dashPassword.isNotEmpty) {
        await authProvider.saveDashboardCredentials(
          username: dashUsername,
          password: dashPassword,
        );
        print('[WiFiSetup] Dashboard credentials saved successfully');
      } else {
        print('[WiFiSetup] Skipping credential save (empty username or password)');
      }

      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text('WiFi credentials saved! Device is connecting...'),
          backgroundColor: Colors.green,
        ),
      );

      // Close the screen after 2 seconds
      Future.delayed(const Duration(seconds: 2), () {
        if (mounted) {
          Navigator.of(context).pop();
        }
      });
    }
  }

  @override
  void dispose() {
    _ssidController.dispose();
    _passwordController.dispose();
    _dashboardUsernameController.dispose();
    _dashboardPasswordController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('WiFi Setup - ${widget.deviceName}'),
        elevation: 0,
      ),
      body: Consumer<WiFiSetupProvider>(
        builder: (context, provider, child) {
          if (provider.currentStep == WiFiSetupStep.connectionCheck) {
            return _buildConnectionCheckView(provider);
          } else if (provider.currentStep == WiFiSetupStep.wifiConfiguration) {
            return _buildWiFiConfigurationView(provider);
          } else if (provider.currentStep == WiFiSetupStep.saving) {
            return _buildSavingView();
          } else {
            return _buildCompletedView();
          }
        },
      ),
    );
  }

  Widget _buildConnectionCheckView(WiFiSetupProvider provider) {
    final theme = Theme.of(context);
    final isDark = theme.brightness == Brightness.dark;

    return Center(
      child: Padding(
        padding: const EdgeInsets.all(24.0),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            if (provider.isLoading)
              const Column(
                children: [
                  CircularProgressIndicator(),
                  SizedBox(height: 24),
                  Text('Connecting to device...'),
                ],
              )
            else if (provider.error != null)
              Column(
                children: [
                  Icon(
                    Icons.wifi_off,
                    size: 80,
                    color: isDark ? const Color(0xFF6B7280) : const Color(0xFF9CA3AF),
                  ),
                  const SizedBox(height: 24),
                  Text(
                    'Cannot Connect to Device',
                    style: theme.textTheme.headlineSmall?.copyWith(
                      fontWeight: FontWeight.bold,
                    ),
                  ),
                  const SizedBox(height: 16),
                  Text(
                    provider.error!,
                    textAlign: TextAlign.center,
                    style: theme.textTheme.bodyMedium?.copyWith(
                      color: Colors.red,
                    ),
                  ),
                  const SizedBox(height: 32),
                  Container(
                    padding: const EdgeInsets.all(16),
                    decoration: BoxDecoration(
                      color: isDark ? const Color(0xFF1F2937) : const Color(0xFFF3F4F6),
                      borderRadius: BorderRadius.circular(8),
                      border: Border.all(
                        color: isDark ? const Color(0xFF374151) : const Color(0xFFE5E7EB),
                      ),
                    ),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          'Setup Instructions:',
                          style: theme.textTheme.titleMedium?.copyWith(
                            fontWeight: FontWeight.bold,
                          ),
                        ),
                        const SizedBox(height: 12),
                        _buildInstructionStep('1', 'Press and hold the setup button on your device for 5 seconds'),
                        const SizedBox(height: 8),
                        _buildInstructionStep('2', 'Device will create a hotspot named "AquaFlow-${widget.deviceId}"'),
                        const SizedBox(height: 8),
                        _buildInstructionStep('3', 'Connect your phone to this hotspot'),
                        const SizedBox(height: 8),
                        _buildInstructionStep('4', 'Return to app and tap "Retry"'),
                      ],
                    ),
                  ),
                  const SizedBox(height: 32),
                  ElevatedButton.icon(
                    onPressed: _checkConnection,
                    icon: const Icon(Icons.refresh),
                    label: const Text('Retry Connection'),
                    style: ElevatedButton.styleFrom(
                      padding: const EdgeInsets.symmetric(horizontal: 32, vertical: 16),
                    ),
                  ),
                ],
              ),
          ],
        ),
      ),
    );
  }

  Widget _buildInstructionStep(String number, String text) {
    final theme = Theme.of(context);
    final isDark = theme.brightness == Brightness.dark;

    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Container(
          width: 24,
          height: 24,
          decoration: BoxDecoration(
            color: theme.primaryColor,
            shape: BoxShape.circle,
          ),
          child: Center(
            child: Text(
              number,
              style: const TextStyle(
                color: Colors.white,
                fontSize: 12,
                fontWeight: FontWeight.bold,
              ),
            ),
          ),
        ),
        const SizedBox(width: 12),
        Expanded(
          child: Text(
            text,
            style: theme.textTheme.bodyMedium?.copyWith(
              color: isDark ? const Color(0xFF9CA3AF) : const Color(0xFF6B7280),
            ),
          ),
        ),
      ],
    );
  }

  Widget _buildWiFiConfigurationView(WiFiSetupProvider provider) {
    final theme = Theme.of(context);
    final isDark = theme.brightness == Brightness.dark;

    return SingleChildScrollView(
      padding: const EdgeInsets.all(16.0),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          // Scan WiFi button
          ElevatedButton.icon(
            onPressed: provider.isScanning ? null : _scanNetworks,
            icon: provider.isScanning
                ? const SizedBox(
                    width: 16,
                    height: 16,
                    child: CircularProgressIndicator(strokeWidth: 2),
                  )
                : const Icon(Icons.wifi_find),
            label: Text(provider.isScanning ? 'Scanning...' : 'Scan WiFi Networks'),
            style: ElevatedButton.styleFrom(
              padding: const EdgeInsets.symmetric(vertical: 16),
            ),
          ),

          const SizedBox(height: 24),

          // Manual entry section
          Text(
            'WiFi Credentials',
            style: theme.textTheme.titleMedium?.copyWith(
              fontWeight: FontWeight.bold,
            ),
          ),
          const SizedBox(height: 12),

          // SSID field
          TextField(
            controller: _ssidController,
            decoration: InputDecoration(
              labelText: 'Network Name (SSID)',
              hintText: 'Enter WiFi network name',
              prefixIcon: const Icon(Icons.wifi),
              border: OutlineInputBorder(
                borderRadius: BorderRadius.circular(8),
              ),
              filled: true,
              fillColor: isDark ? const Color(0xFF1F2937) : Colors.white,
            ),
            onChanged: (value) => provider.setSelectedSSID(value),
          ),

          const SizedBox(height: 16),

          // Password field
          TextField(
            controller: _passwordController,
            obscureText: !provider.isPasswordVisible,
            decoration: InputDecoration(
              labelText: 'WiFi Password',
              hintText: 'Enter WiFi password',
              prefixIcon: const Icon(Icons.lock),
              suffixIcon: IconButton(
                icon: Icon(
                  provider.isPasswordVisible ? Icons.visibility_off : Icons.visibility,
                ),
                onPressed: provider.togglePasswordVisibility,
              ),
              border: OutlineInputBorder(
                borderRadius: BorderRadius.circular(8),
              ),
              filled: true,
              fillColor: isDark ? const Color(0xFF1F2937) : Colors.white,
            ),
            onChanged: (value) => provider.setPassword(value),
          ),

          // Dashboard Credentials section (hidden in offline mode)
          if (!_isOfflineMode) ...[
            const SizedBox(height: 24),

            Text(
              'Dashboard Credentials',
              style: theme.textTheme.titleMedium?.copyWith(
                fontWeight: FontWeight.bold,
              ),
            ),
            const SizedBox(height: 12),

            // Dashboard Username field
            TextField(
              controller: _dashboardUsernameController,
              decoration: InputDecoration(
                labelText: 'Dashboard Username',
                hintText: 'Enter dashboard username',
                prefixIcon: const Icon(Icons.person),
                border: OutlineInputBorder(
                  borderRadius: BorderRadius.circular(8),
                ),
                filled: true,
                fillColor: isDark ? const Color(0xFF1F2937) : Colors.white,
              ),
              onChanged: (value) => provider.setDashboardUsername(value),
            ),

            const SizedBox(height: 16),

            // Dashboard Password field
            TextField(
              controller: _dashboardPasswordController,
              obscureText: !provider.isDashboardPasswordVisible,
              decoration: InputDecoration(
                labelText: 'Dashboard Password',
                hintText: 'Enter dashboard password',
                prefixIcon: const Icon(Icons.vpn_key),
                suffixIcon: IconButton(
                  icon: Icon(
                    provider.isDashboardPasswordVisible ? Icons.visibility_off : Icons.visibility,
                  ),
                  onPressed: provider.toggleDashboardPasswordVisibility,
                ),
                border: OutlineInputBorder(
                  borderRadius: BorderRadius.circular(8),
                ),
                filled: true,
                fillColor: isDark ? const Color(0xFF1F2937) : Colors.white,
              ),
              onChanged: (value) => provider.setDashboardPassword(value),
            ),

            const SizedBox(height: 24),
          ] else ...[
            const SizedBox(height: 24),
          ],

          // Error message
          if (provider.error != null)
            Container(
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: Colors.red.withOpacity(0.1),
                border: Border.all(color: Colors.red),
                borderRadius: BorderRadius.circular(8),
              ),
              child: Row(
                children: [
                  const Icon(Icons.error, color: Colors.red),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Text(
                      provider.error!,
                      style: const TextStyle(color: Colors.red),
                    ),
                  ),
                ],
              ),
            ),

          if (provider.error != null) const SizedBox(height: 16),

          // Save button
          ElevatedButton.icon(
            onPressed: provider.isLoading ? null : _saveCredentials,
            icon: const Icon(Icons.save),
            label: const Text('Save & Connect'),
            style: ElevatedButton.styleFrom(
              padding: const EdgeInsets.symmetric(vertical: 16),
              backgroundColor: Colors.green,
              foregroundColor: Colors.white,
            ),
          ),

          const SizedBox(height: 16),

          // Cancel button
          OutlinedButton.icon(
            onPressed: () => Navigator.of(context).pop(),
            icon: const Icon(Icons.close),
            label: const Text('Cancel'),
            style: OutlinedButton.styleFrom(
              padding: const EdgeInsets.symmetric(vertical: 16),
            ),
          ),

          // Network list - shown below action buttons
          if (provider.availableNetworks.isNotEmpty) ...[
            const SizedBox(height: 32),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Text(
                  'Available Networks',
                  style: theme.textTheme.titleMedium?.copyWith(
                    fontWeight: FontWeight.bold,
                  ),
                ),
                Text(
                  '${provider.availableNetworks.length} found',
                  style: theme.textTheme.bodySmall?.copyWith(
                    color: isDark ? const Color(0xFF9CA3AF) : const Color(0xFF6B7280),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),
            ...provider.availableNetworks.map((network) {
              final isSelected = _ssidController.text == network.ssid;
              return NetworkListTile(
                network: network,
                isSelected: isSelected,
                onTap: () {
                  setState(() {
                    _ssidController.text = network.ssid;
                    provider.setSelectedSSID(network.ssid);
                  });
                },
              );
            }).toList(),
          ],
        ],
      ),
    );
  }

  Widget _buildSavingView() {
    return const Center(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          CircularProgressIndicator(),
          SizedBox(height: 24),
          Text('Saving WiFi credentials...'),
          SizedBox(height: 8),
          Text(
            'Device is connecting to WiFi',
            style: TextStyle(fontSize: 12, color: Colors.grey),
          ),
        ],
      ),
    );
  }

  Widget _buildCompletedView() {
    return Center(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          const Icon(
            Icons.check_circle,
            size: 80,
            color: Colors.green,
          ),
          const SizedBox(height: 24),
          const Text(
            'WiFi Setup Complete!',
            style: TextStyle(
              fontSize: 24,
              fontWeight: FontWeight.bold,
            ),
          ),
          const SizedBox(height: 8),
          const Text(
            'Device is connecting to WiFi...',
            style: TextStyle(fontSize: 14, color: Colors.grey),
          ),
          const SizedBox(height: 32),
          ElevatedButton(
            onPressed: () => Navigator.of(context).pop(),
            child: const Text('Done'),
          ),
        ],
      ),
    );
  }
}
