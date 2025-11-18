import 'package:flutter/material.dart';

/// App theme configurations for dark and light modes
class AppThemes {
  // Dark Theme Colors
  static const Color darkBackground1 = Color(0xFF1F2937); // gray-800
  static const Color darkBackground2 = Color(0xFF000000); // black
  static const Color darkCard = Color(0xFF1F2937); // gray-800
  static const Color darkCardBorder = Color(0xFF374151); // gray-700
  static const Color darkPrimary = Color(0xFF22D3EE); // cyan-400
  static const Color darkPrimaryDark = Color(0xFF06B6D4); // cyan-500
  static const Color darkTextPrimary = Color(0xFFFFFFFF); // white
  static const Color darkTextSecondary = Color(0xFF9CA3AF); // gray-400

  // Light Theme Colors
  static const Color lightBackground1 = Color(0xFFFFFFFF); // white
  static const Color lightBackground2 = Color(0xFFECFEFF); // cyan-50
  static const Color lightCard = Color(0xFFFFFFFF); // white
  static const Color lightCardBorder = Color(0xFFE5E7EB); // gray-200
  static const Color lightPrimary = Color(0xFF0891B2); // cyan-600
  static const Color lightPrimaryDark = Color(0xFF0E7490); // cyan-700
  static const Color lightTextPrimary = Color(0xFF111827); // gray-900
  static const Color lightTextSecondary = Color(0xFF4B5563); // gray-600

  /// Dark theme configuration
  static ThemeData get darkTheme {
    return ThemeData(
      useMaterial3: true,
      brightness: Brightness.dark,
      scaffoldBackgroundColor: darkBackground1,

      // Primary color scheme
      colorScheme: const ColorScheme.dark(
        primary: darkPrimary,
        secondary: darkPrimaryDark,
        surface: darkCard,
        background: darkBackground1,
        onPrimary: Color(0xFF000000),
        onSecondary: Color(0xFF000000),
        onSurface: darkTextPrimary,
        onBackground: darkTextPrimary,
        outline: darkCardBorder,
      ),

      // Card theme
      cardTheme: CardThemeData(
        color: darkCard,
        elevation: 0,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(16),
          side: const BorderSide(color: darkCardBorder, width: 1),
        ),
        margin: const EdgeInsets.symmetric(horizontal: 8, vertical: 6),
      ),

      // AppBar theme
      appBarTheme: const AppBarTheme(
        backgroundColor: Colors.transparent,
        elevation: 0,
        centerTitle: false,
        titleTextStyle: TextStyle(
          color: darkTextPrimary,
          fontSize: 24,
          fontWeight: FontWeight.bold,
          letterSpacing: -0.5,
        ),
        iconTheme: IconThemeData(color: darkPrimary),
      ),

      // Text theme
      textTheme: const TextTheme(
        displayLarge: TextStyle(
          fontSize: 48,
          fontWeight: FontWeight.bold,
          color: darkTextPrimary,
        ),
        displayMedium: TextStyle(
          fontSize: 36,
          fontWeight: FontWeight.bold,
          color: darkTextPrimary,
        ),
        displaySmall: TextStyle(
          fontSize: 24,
          fontWeight: FontWeight.bold,
          color: darkTextPrimary,
        ),
        headlineMedium: TextStyle(
          fontSize: 20,
          fontWeight: FontWeight.w600,
          color: darkTextPrimary,
        ),
        titleLarge: TextStyle(
          fontSize: 18,
          fontWeight: FontWeight.w600,
          color: darkTextPrimary,
        ),
        titleMedium: TextStyle(
          fontSize: 16,
          fontWeight: FontWeight.w500,
          color: darkTextPrimary,
        ),
        bodyLarge: TextStyle(
          fontSize: 16,
          color: darkTextPrimary,
        ),
        bodyMedium: TextStyle(
          fontSize: 14,
          color: darkTextSecondary,
        ),
        labelLarge: TextStyle(
          fontSize: 12,
          color: darkTextSecondary,
          fontWeight: FontWeight.w500,
        ),
      ),

      // Icon theme
      iconTheme: const IconThemeData(
        color: darkPrimary,
        size: 24,
      ),

      // Button themes
      elevatedButtonTheme: ElevatedButtonThemeData(
        style: ElevatedButton.styleFrom(
          backgroundColor: darkPrimary,
          foregroundColor: Colors.black,
          elevation: 0,
          padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 12),
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(12),
          ),
        ),
      ),

      floatingActionButtonTheme: const FloatingActionButtonThemeData(
        backgroundColor: darkPrimary,
        foregroundColor: Colors.black,
      ),
    );
  }

  /// Light theme configuration
  static ThemeData get lightTheme {
    return ThemeData(
      useMaterial3: true,
      brightness: Brightness.light,
      scaffoldBackgroundColor: lightBackground1,

      // Primary color scheme
      colorScheme: const ColorScheme.light(
        primary: lightPrimary,
        secondary: lightPrimaryDark,
        surface: lightCard,
        background: lightBackground1,
        onPrimary: Color(0xFFFFFFFF),
        onSecondary: Color(0xFFFFFFFF),
        onSurface: lightTextPrimary,
        onBackground: lightTextPrimary,
        outline: lightCardBorder,
      ),

      // Card theme
      cardTheme: CardThemeData(
        color: lightCard,
        elevation: 0,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(16),
          side: const BorderSide(color: lightCardBorder, width: 1),
        ),
        margin: const EdgeInsets.symmetric(horizontal: 8, vertical: 6),
      ),

      // AppBar theme
      appBarTheme: const AppBarTheme(
        backgroundColor: Colors.transparent,
        elevation: 0,
        centerTitle: false,
        titleTextStyle: TextStyle(
          color: lightTextPrimary,
          fontSize: 24,
          fontWeight: FontWeight.bold,
          letterSpacing: -0.5,
        ),
        iconTheme: IconThemeData(color: lightPrimary),
      ),

      // Text theme
      textTheme: const TextTheme(
        displayLarge: TextStyle(
          fontSize: 48,
          fontWeight: FontWeight.bold,
          color: lightTextPrimary,
        ),
        displayMedium: TextStyle(
          fontSize: 36,
          fontWeight: FontWeight.bold,
          color: lightTextPrimary,
        ),
        displaySmall: TextStyle(
          fontSize: 24,
          fontWeight: FontWeight.bold,
          color: lightTextPrimary,
        ),
        headlineMedium: TextStyle(
          fontSize: 20,
          fontWeight: FontWeight.w600,
          color: lightTextPrimary,
        ),
        titleLarge: TextStyle(
          fontSize: 18,
          fontWeight: FontWeight.w600,
          color: lightTextPrimary,
        ),
        titleMedium: TextStyle(
          fontSize: 16,
          fontWeight: FontWeight.w500,
          color: lightTextPrimary,
        ),
        bodyLarge: TextStyle(
          fontSize: 16,
          color: lightTextPrimary,
        ),
        bodyMedium: TextStyle(
          fontSize: 14,
          color: lightTextSecondary,
        ),
        labelLarge: TextStyle(
          fontSize: 12,
          color: lightTextSecondary,
          fontWeight: FontWeight.w500,
        ),
      ),

      // Icon theme
      iconTheme: const IconThemeData(
        color: lightPrimary,
        size: 24,
      ),

      // Button themes
      elevatedButtonTheme: ElevatedButtonThemeData(
        style: ElevatedButton.styleFrom(
          backgroundColor: lightPrimary,
          foregroundColor: Colors.white,
          elevation: 0,
          padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 12),
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(12),
          ),
        ),
      ),

      floatingActionButtonTheme: const FloatingActionButtonThemeData(
        backgroundColor: lightPrimary,
        foregroundColor: Colors.white,
      ),
    );
  }
}
