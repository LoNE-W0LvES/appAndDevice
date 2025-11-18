/// Application configuration
/// This file contains all configurable settings for the IoT app
/// Modify these values to customize for different projects
class AppConfig {
  // API Configuration
  static const String baseUrl = 'http://103.136.236.16';
  static const String apiPrefix = '/api';

  // API Endpoints
  static const String signInEndpoint = '$apiPrefix/auth/sign-in/email';
  static const String signUpEndpoint = '$apiPrefix/auth/sign-up/email';
  static const String signOutEndpoint = '$apiPrefix/auth/sign-out';
  static const String devicesEndpoint = '$apiPrefix/devices';

  // Session Configuration
  static const String sessionCookieName = 'better-auth.session_token';
  static const String sessionStorageKey = 'user_session';
  static const String keepLoggedInKey = 'keep_logged_in';
  static const String dashboardUsernameKey = 'dashboard_username';
  static const String dashboardPasswordKey = 'dashboard_password';

  // Offline Mode Configuration
  static const String offlineModeKey = 'offline_mode_enabled';
  static const String offlineDevicesKey = 'offline_devices'; // JSON string of offline devices

  // App Configuration
  static const String appName = 'IoT Water Tank';
  static const String appVersion = '1.0.0';

  // Project Filter - Set this to filter devices by specific project
  // Set to null to show all projects
  static const String? projectId = 'wt001';

  // Timeouts (in seconds)
  static const int connectionTimeout = 30;
  static const int receiveTimeout = 30;

  // Refresh Configuration
  static const int autoRefreshInterval = 30; // seconds

  // Device Status
  static const int deviceActiveThreshold = 300; // seconds (5 minutes)

  // UI Configuration
  static const int maxRetryAttempts = 3;

  // ========================================
  // DEBUG CONFIGURATION
  // ========================================
  /// Master debug flag - set to false to disable all console logging
  static const bool enableDebugMode = true;

  /// Enable/disable specific log categories
  static const bool logApiCalls = true;
  static const bool logOfflineMode = true;
  static const bool logDeviceUpdates = true;
  static const bool logAuth = true;
  static const bool logTheme = true;
  static const bool logConfig = true;

  // ========================================
  // DEBUG LOGGING METHODS
  // ========================================

  /// General debug print - only prints when enableDebugMode is true
  static void debugPrint(String message, {String? tag}) {
    if (enableDebugMode) {
      final prefix = tag != null ? '[$tag] ' : '';
      // ignore: avoid_print
      print('$prefix$message');
    }
  }

  /// API debug print
  static void apiLog(String message) {
    if (enableDebugMode && logApiCalls) {
      // ignore: avoid_print
      print('[API] $message');
    }
  }

  /// Offline mode debug print
  static void offlineLog(String message) {
    if (enableDebugMode && logOfflineMode) {
      // ignore: avoid_print
      print('[OFFLINE] $message');
    }
  }

  /// Device update debug print
  static void deviceLog(String message) {
    if (enableDebugMode && logDeviceUpdates) {
      // ignore: avoid_print
      print('[DEVICE] $message');
    }
  }

  /// Auth debug print
  static void authLog(String message) {
    if (enableDebugMode && logAuth) {
      // ignore: avoid_print
      print('[AUTH] $message');
    }
  }

  /// Theme debug print
  static void themeLog(String message) {
    if (enableDebugMode && logTheme) {
      // ignore: avoid_print
      print('[THEME] $message');
    }
  }

  /// Config debug print
  static void configLog(String message) {
    if (enableDebugMode && logConfig) {
      // ignore: avoid_print
      print('[CONFIG] $message');
    }
  }

  /// Error log - always prints regardless of enableDebugMode
  static void errorLog(String message, {dynamic error, StackTrace? stackTrace}) {
    // ignore: avoid_print
    print('[ERROR] $message');
    if (error != null) {
      // ignore: avoid_print
      print('Error details: $error');
    }
    if (stackTrace != null && enableDebugMode) {
      // ignore: avoid_print
      print('Stack trace: $stackTrace');
    }
  }

  // System Control Data Keys (these are typically not user-editable)
  static const List<String> systemControlKeys = [
    'force_update',
    'config_update',
  ];

  /// Check if a control key is a system key
  static bool isSystemControl(String key) {
    return systemControlKeys.contains(key);
  }

  /// Get full URL for an endpoint
  static String getUrl(String endpoint) {
    return '$baseUrl$endpoint';
  }
}
