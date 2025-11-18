import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'providers/auth_provider.dart';
import 'providers/device_provider.dart';
import 'providers/offline_provider.dart';
import 'providers/theme_provider.dart' as app_theme;
import 'providers/wifi_setup_provider.dart';
import 'providers/timestamp_provider.dart';
import 'screens/login_screen.dart';
import 'screens/device_list_screen.dart';
import 'config/app_config.dart';
import 'theme/app_themes.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();

  // Initialize providers
  final authProvider = AuthProvider();
  final deviceProvider = DeviceProvider();
  final offlineProvider = OfflineProvider();
  final themeProvider = app_theme.ThemeProvider();
  final wifiSetupProvider = WiFiSetupProvider();
  final timestampProvider = TimestampProvider();

  await authProvider.initialize();
  await deviceProvider.initialize();
  await offlineProvider.initialize();
  await themeProvider.initialize();

  runApp(
    MultiProvider(
      providers: [
        ChangeNotifierProvider.value(value: authProvider),
        ChangeNotifierProvider.value(value: deviceProvider),
        ChangeNotifierProvider.value(value: offlineProvider),
        ChangeNotifierProvider.value(value: themeProvider),
        ChangeNotifierProvider.value(value: wifiSetupProvider),
        ChangeNotifierProvider.value(value: timestampProvider),
      ],
      child: const MyApp(),
    ),
  );
}

class MyApp extends StatelessWidget {
  const MyApp({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Consumer<app_theme.ThemeProvider>(
      builder: (context, themeProvider, child) {
        // Update system brightness if using system theme
        themeProvider.updateSystemBrightness(
          MediaQuery.of(context).platformBrightness,
        );

        return MaterialApp(
          title: AppConfig.appName,
          debugShowCheckedModeBanner: false,
          theme: AppThemes.lightTheme,
          darkTheme: AppThemes.darkTheme,
          themeMode: _getThemeMode(themeProvider.themeMode),
          home: const AuthWrapper(),
        );
      },
    );
  }

  /// Convert app ThemeMode to Flutter ThemeMode
  ThemeMode _getThemeMode(app_theme.ThemeMode mode) {
    switch (mode) {
      case app_theme.ThemeMode.light:
        return ThemeMode.light;
      case app_theme.ThemeMode.dark:
        return ThemeMode.dark;
      case app_theme.ThemeMode.system:
        return ThemeMode.system;
    }
  }
}

/// Wrapper to handle authentication state
class AuthWrapper extends StatelessWidget {
  const AuthWrapper({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Consumer<AuthProvider>(
      builder: (context, authProvider, child) {
        // Show login screen if not authenticated
        if (!authProvider.isAuthenticated) {
          return const LoginScreen();
        }

        // Show device list if authenticated
        return const DeviceListScreen();
      },
    );
  }
}
