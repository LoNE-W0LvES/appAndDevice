import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';

/// Theme modes available in the app
enum ThemeMode { light, dark, system }

/// Provider for managing app theme
class ThemeProvider extends ChangeNotifier {
  static const String _themeKey = 'app_theme_mode';

  ThemeMode _themeMode = ThemeMode.dark; // Default to dark
  Brightness _systemBrightness = Brightness.light;

  ThemeMode get themeMode => _themeMode;
  Brightness get systemBrightness => _systemBrightness;

  /// Get the effective brightness based on theme mode
  Brightness get effectiveBrightness {
    switch (_themeMode) {
      case ThemeMode.light:
        return Brightness.light;
      case ThemeMode.dark:
        return Brightness.dark;
      case ThemeMode.system:
        return _systemBrightness;
    }
  }

  /// Check if currently using dark theme
  bool get isDarkMode => effectiveBrightness == Brightness.dark;

  /// Initialize theme from saved preferences
  Future<void> initialize() async {
    final prefs = await SharedPreferences.getInstance();
    final savedTheme = prefs.getString(_themeKey);

    if (savedTheme != null) {
      _themeMode = ThemeMode.values.firstWhere(
        (e) => e.toString() == savedTheme,
        orElse: () => ThemeMode.dark,
      );
      notifyListeners();
    }
  }

  /// Update system brightness (called when device theme changes)
  void updateSystemBrightness(Brightness brightness) {
    if (_systemBrightness != brightness) {
      _systemBrightness = brightness;
      if (_themeMode == ThemeMode.system) {
        notifyListeners();
      }
    }
  }

  /// Set theme mode and save to preferences
  Future<void> setThemeMode(ThemeMode mode) async {
    if (_themeMode != mode) {
      _themeMode = mode;
      notifyListeners();

      final prefs = await SharedPreferences.getInstance();
      await prefs.setString(_themeKey, mode.toString());
    }
  }

  /// Toggle between light and dark (ignores system)
  Future<void> toggleTheme() async {
    final newMode = isDarkMode ? ThemeMode.light : ThemeMode.dark;
    await setThemeMode(newMode);
  }
}
