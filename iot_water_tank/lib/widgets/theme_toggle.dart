import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/theme_provider.dart' as app_theme;

/// Widget for toggling between light, dark, and system themes
class ThemeToggle extends StatelessWidget {
  final bool showLabel;

  const ThemeToggle({
    Key? key,
    this.showLabel = false,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Consumer<app_theme.ThemeProvider>(
      builder: (context, themeProvider, child) {
        return PopupMenuButton<app_theme.ThemeMode>(
          icon: Icon(
            _getThemeIcon(themeProvider.themeMode),
            color: Theme.of(context).colorScheme.primary,
          ),
          tooltip: 'Change Theme',
          onSelected: (mode) => themeProvider.setThemeMode(mode),
          itemBuilder: (context) => [
            PopupMenuItem(
              value: app_theme.ThemeMode.light,
              child: Row(
                children: [
                  Icon(
                    Icons.light_mode,
                    color: themeProvider.themeMode == app_theme.ThemeMode.light
                        ? Theme.of(context).colorScheme.primary
                        : null,
                  ),
                  const SizedBox(width: 12),
                  const Text('Light'),
                  if (themeProvider.themeMode == app_theme.ThemeMode.light)
                    Padding(
                      padding: const EdgeInsets.only(left: 8),
                      child: Icon(
                        Icons.check,
                        color: Theme.of(context).colorScheme.primary,
                        size: 18,
                      ),
                    ),
                ],
              ),
            ),
            PopupMenuItem(
              value: app_theme.ThemeMode.dark,
              child: Row(
                children: [
                  Icon(
                    Icons.dark_mode,
                    color: themeProvider.themeMode == app_theme.ThemeMode.dark
                        ? Theme.of(context).colorScheme.primary
                        : null,
                  ),
                  const SizedBox(width: 12),
                  const Text('Dark'),
                  if (themeProvider.themeMode == app_theme.ThemeMode.dark)
                    Padding(
                      padding: const EdgeInsets.only(left: 8),
                      child: Icon(
                        Icons.check,
                        color: Theme.of(context).colorScheme.primary,
                        size: 18,
                      ),
                    ),
                ],
              ),
            ),
            PopupMenuItem(
              value: app_theme.ThemeMode.system,
              child: Row(
                children: [
                  Icon(
                    Icons.brightness_auto,
                    color: themeProvider.themeMode == app_theme.ThemeMode.system
                        ? Theme.of(context).colorScheme.primary
                        : null,
                  ),
                  const SizedBox(width: 12),
                  const Text('System'),
                  if (themeProvider.themeMode == app_theme.ThemeMode.system)
                    Padding(
                      padding: const EdgeInsets.only(left: 8),
                      child: Icon(
                        Icons.check,
                        color: Theme.of(context).colorScheme.primary,
                        size: 18,
                      ),
                    ),
                ],
              ),
            ),
          ],
        );
      },
    );
  }

  IconData _getThemeIcon(app_theme.ThemeMode mode) {
    switch (mode) {
      case app_theme.ThemeMode.light:
        return Icons.light_mode;
      case app_theme.ThemeMode.dark:
        return Icons.dark_mode;
      case app_theme.ThemeMode.system:
        return Icons.brightness_auto;
    }
  }
}

/// Simple icon button for quick toggle between light and dark
class QuickThemeToggle extends StatelessWidget {
  const QuickThemeToggle({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Consumer<app_theme.ThemeProvider>(
      builder: (context, themeProvider, child) {
        return IconButton(
          icon: Icon(
            themeProvider.isDarkMode ? Icons.light_mode : Icons.dark_mode,
            color: Theme.of(context).colorScheme.primary,
          ),
          tooltip: themeProvider.isDarkMode ? 'Switch to Light Mode' : 'Switch to Dark Mode',
          onPressed: () => themeProvider.toggleTheme(),
        );
      },
    );
  }
}
