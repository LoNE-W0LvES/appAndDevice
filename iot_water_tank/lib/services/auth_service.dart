import 'dart:io';
import 'package:shared_preferences/shared_preferences.dart';
import '../config/app_config.dart';
import 'api_client.dart';
import '../utils/api_exception.dart';

/// Authentication service for managing user sessions
class AuthService {
  final ApiClient _apiClient = ApiClient();
  SharedPreferences? _prefs;

  /// Initialize the auth service
  Future<void> initialize() async {
    _prefs = await SharedPreferences.getInstance();
    await _apiClient.initialize();
  }

  /// Sign in with email and password
  Future<Map<String, dynamic>> signIn({
    required String email,
    required String password,
    bool keepLoggedIn = false,
  }) async {
    try {
      final response = await _apiClient.post(
        AppConfig.signInEndpoint,
        data: {
          'email': email,
          'password': password,
        },
      );

      if (response.statusCode == 200) {
        // Check if we have the session cookie
        final cookies = await _apiClient.getCookies(AppConfig.baseUrl);
        final sessionCookie = cookies.firstWhere(
          (cookie) => cookie.name == AppConfig.sessionCookieName,
          orElse: () => Cookie('', ''),
        );

        if (sessionCookie.value.isNotEmpty) {
          // Save session state
          await _saveSessionState(true);

          // Save keep logged in preference
          await _saveKeepLoggedIn(keepLoggedIn);

          // Save login credentials as dashboard credentials for WiFi setup autofill
          await saveDashboardCredentials(
            username: email,
            password: password,
          );
          print('[Auth] Login credentials saved as dashboard credentials');

          // Return user data if available
          final data = response.data;
          if (data is Map<String, dynamic>) {
            return data;
          }
        }
      }

      throw ApiException(
        message: 'Login failed. Please check your credentials.',
        statusCode: response.statusCode,
      );
    } catch (e) {
      if (e is ApiException) rethrow;
      throw ApiException.fromError(e);
    }
  }

  /// Sign up with email and password
  Future<Map<String, dynamic>> signUp({
    required String email,
    required String password,
    required String name,
  }) async {
    try {
      final response = await _apiClient.post(
        AppConfig.signUpEndpoint,
        data: {
          'email': email,
          'password': password,
          'name': name,
        },
      );

      if (response.statusCode == 200 || response.statusCode == 201) {
        // Check if we have the session cookie
        final cookies = await _apiClient.getCookies(AppConfig.baseUrl);
        final sessionCookie = cookies.firstWhere(
          (cookie) => cookie.name == AppConfig.sessionCookieName,
          orElse: () => Cookie('', ''),
        );

        if (sessionCookie.value.isNotEmpty) {
          // Save session state
          await _saveSessionState(true);

          // Save signup credentials as dashboard credentials for WiFi setup autofill
          await saveDashboardCredentials(
            username: email,
            password: password,
          );
          print('[Auth] Signup credentials saved as dashboard credentials');

          // Return user data if available
          final data = response.data;
          if (data is Map<String, dynamic>) {
            return data;
          }
        }
      }

      throw ApiException(
        message: 'Sign up failed. Please try again.',
        statusCode: response.statusCode,
      );
    } catch (e) {
      if (e is ApiException) rethrow;
      throw ApiException.fromError(e);
    }
  }

  /// Sign out
  Future<void> signOut() async {
    try {
      // Try to call the sign out endpoint
      try {
        await _apiClient.post(AppConfig.signOutEndpoint);
      } catch (e) {
        // Ignore errors from sign out endpoint
        print('Sign out endpoint error: $e');
      }

      // Clear local session data
      await _apiClient.clearCookies();
      await _saveSessionState(false);
      await _saveKeepLoggedIn(false); // Clear "keep logged in" preference
    } catch (e) {
      throw ApiException.fromError(e);
    }
  }

  /// Check if user is logged in
  Future<bool> isLoggedIn() async {
    try {
      _prefs ??= await SharedPreferences.getInstance();

      // Check if user chose to stay logged in
      final keepLoggedIn = _prefs?.getBool(AppConfig.keepLoggedInKey) ?? false;
      print('[Auth] Checking login status: keepLoggedIn=$keepLoggedIn');

      // If user didn't choose "keep me logged in", treat as logged out
      if (!keepLoggedIn) {
        print('[Auth] Keep me logged in is false, logging out');
        await _saveSessionState(false);
        return false;
      }

      final isLoggedIn = _prefs?.getBool(AppConfig.sessionStorageKey) ?? false;
      print('[Auth] Session storage key: $isLoggedIn');

      if (!isLoggedIn) {
        print('[Auth] No session found in storage');
        return false;
      }

      // Check if we have valid cookies
      final cookies = await _apiClient.getCookies(AppConfig.baseUrl);
      final sessionCookie = cookies.firstWhere(
        (cookie) => cookie.name == AppConfig.sessionCookieName,
        orElse: () => Cookie('', ''),
      );

      print('[Auth] Session cookie found: ${sessionCookie.value.isNotEmpty}');
      print('[Auth] Session cookie value length: ${sessionCookie.value.length}');

      return sessionCookie.value.isNotEmpty;
    } catch (e) {
      print('[Auth] Error checking login status: $e');
      return false;
    }
  }

  /// Save session state to local storage
  Future<void> _saveSessionState(bool isLoggedIn) async {
    _prefs ??= await SharedPreferences.getInstance();
    await _prefs?.setBool(AppConfig.sessionStorageKey, isLoggedIn);
  }

  /// Clear all session data
  Future<void> clearSession() async {
    await _apiClient.clearCookies();
    await _saveSessionState(false);
  }

  /// Save keep logged in preference
  Future<void> _saveKeepLoggedIn(bool keepLoggedIn) async {
    _prefs ??= await SharedPreferences.getInstance();
    await _prefs?.setBool(AppConfig.keepLoggedInKey, keepLoggedIn);
    print('[Auth] Keep me logged in preference saved: $keepLoggedIn');
  }

  /// Get keep logged in preference
  Future<bool> getKeepLoggedIn() async {
    _prefs ??= await SharedPreferences.getInstance();
    return _prefs?.getBool(AppConfig.keepLoggedInKey) ?? false;
  }

  /// Save dashboard credentials
  Future<void> saveDashboardCredentials({
    required String username,
    required String password,
  }) async {
    _prefs ??= await SharedPreferences.getInstance();
    await _prefs?.setString(AppConfig.dashboardUsernameKey, username);
    await _prefs?.setString(AppConfig.dashboardPasswordKey, password);
  }

  /// Get dashboard username
  Future<String?> getDashboardUsername() async {
    _prefs ??= await SharedPreferences.getInstance();
    return _prefs?.getString(AppConfig.dashboardUsernameKey);
  }

  /// Get dashboard password
  Future<String?> getDashboardPassword() async {
    _prefs ??= await SharedPreferences.getInstance();
    return _prefs?.getString(AppConfig.dashboardPasswordKey);
  }

  /// Clear dashboard credentials
  Future<void> clearDashboardCredentials() async {
    _prefs ??= await SharedPreferences.getInstance();
    await _prefs?.remove(AppConfig.dashboardUsernameKey);
    await _prefs?.remove(AppConfig.dashboardPasswordKey);
  }
}
