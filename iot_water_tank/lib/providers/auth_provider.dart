import 'package:flutter/foundation.dart';
import '../services/auth_service.dart';
import '../utils/api_exception.dart';

/// Provider for authentication state management
class AuthProvider with ChangeNotifier {
  final AuthService _authService = AuthService();

  bool _isAuthenticated = false;
  bool _isLoading = false;
  String? _error;
  Map<String, dynamic>? _userData;

  bool get isAuthenticated => _isAuthenticated;
  bool get isLoading => _isLoading;
  String? get error => _error;
  Map<String, dynamic>? get userData => _userData;

  /// Initialize the auth provider
  Future<void> initialize() async {
    _setLoading(true);
    try {
      await _authService.initialize();
      _isAuthenticated = await _authService.isLoggedIn();
      _setError(null);
    } catch (e) {
      _setError('Failed to initialize authentication');
      _isAuthenticated = false;
    } finally {
      _setLoading(false);
    }
  }

  /// Sign in with email and password
  Future<bool> signIn({
    required String email,
    required String password,
    bool keepLoggedIn = false,
  }) async {
    _setLoading(true);
    _setError(null);

    try {
      _userData = await _authService.signIn(
        email: email,
        password: password,
        keepLoggedIn: keepLoggedIn,
      );
      _isAuthenticated = true;
      _setLoading(false);
      notifyListeners();
      return true;
    } on ApiException catch (e) {
      _setError(e.message);
      _isAuthenticated = false;
      _setLoading(false);
      return false;
    } catch (e) {
      _setError('An unexpected error occurred');
      _isAuthenticated = false;
      _setLoading(false);
      return false;
    }
  }

  /// Sign up with email and password
  Future<bool> signUp({
    required String email,
    required String password,
    required String name,
  }) async {
    _setLoading(true);
    _setError(null);

    try {
      _userData = await _authService.signUp(
        email: email,
        password: password,
        name: name,
      );
      _isAuthenticated = true;
      _setLoading(false);
      notifyListeners();
      return true;
    } on ApiException catch (e) {
      _setError(e.message);
      _isAuthenticated = false;
      _setLoading(false);
      return false;
    } catch (e) {
      _setError('An unexpected error occurred');
      _isAuthenticated = false;
      _setLoading(false);
      return false;
    }
  }

  /// Sign out
  Future<void> signOut() async {
    _setLoading(true);
    try {
      await _authService.signOut();
      _isAuthenticated = false;
      _userData = null;
      _setError(null);
    } catch (e) {
      _setError('Failed to sign out');
    } finally {
      _setLoading(false);
    }
  }

  /// Clear error message
  void clearError() {
    _setError(null);
  }

  /// Check login status
  Future<bool> checkLoginStatus() async {
    try {
      _isAuthenticated = await _authService.isLoggedIn();
      notifyListeners();
      return _isAuthenticated;
    } catch (e) {
      _isAuthenticated = false;
      notifyListeners();
      return false;
    }
  }

  void _setLoading(bool loading) {
    _isLoading = loading;
    notifyListeners();
  }

  void _setError(String? error) {
    _error = error;
    notifyListeners();
  }

  /// Save dashboard credentials
  Future<void> saveDashboardCredentials({
    required String username,
    required String password,
  }) async {
    await _authService.saveDashboardCredentials(
      username: username,
      password: password,
    );
  }

  /// Get dashboard username
  Future<String?> getDashboardUsername() async {
    return await _authService.getDashboardUsername();
  }

  /// Get dashboard password
  Future<String?> getDashboardPassword() async {
    return await _authService.getDashboardPassword();
  }

  /// Clear dashboard credentials
  Future<void> clearDashboardCredentials() async {
    await _authService.clearDashboardCredentials();
  }
}
