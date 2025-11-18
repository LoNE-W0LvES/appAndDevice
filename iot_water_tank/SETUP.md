# Flutter Project Setup

This Flutter project was created manually and requires some additional setup steps to complete the platform-specific configurations.

## Important: Complete Platform Setup

Since this project was initialized manually (without `flutter create`), you need to complete the platform configurations on your local machine.

### Option 1: Quick Setup (Recommended)

The easiest way is to let Flutter generate the missing platform files:

1. **Backup your code** (optional safety measure):
   ```bash
   cp -r lib lib_backup
   ```

2. **Run Flutter create** to generate platform files:
   ```bash
   flutter create .
   ```

   This will generate:
   - Android project files (android/)
   - iOS project files (ios/)
   - Web files (web/)
   - Other necessary configurations

   **Important**: This will NOT overwrite your existing code in `lib/` or `pubspec.yaml`!

3. **Get dependencies**:
   ```bash
   flutter pub get
   ```

4. **Run the app**:
   ```bash
   flutter run
   ```

### Option 2: Manual Platform Setup

If you prefer more control, you can set up each platform manually:

#### Android Setup

1. Create a new Flutter project in a temporary directory:
   ```bash
   cd /tmp
   flutter create temp_project
   ```

2. Copy the Android directory:
   ```bash
   cp -r temp_project/android /path/to/IOTWaterTank/
   ```

3. Update `android/app/build.gradle`:
   - Set `applicationId "com.iot.iot_water_tank"`
   - Update app name if needed

#### iOS Setup

1. Copy iOS directory from temp project:
   ```bash
   cp -r temp_project/ios /path/to/IOTWaterTank/
   ```

2. Update bundle identifier in Xcode

#### Web Setup

1. Copy web directory:
   ```bash
   cp -r temp_project/web /path/to/IOTWaterTank/
   ```

## Verification

After setup, verify everything works:

```bash
# Check for issues
flutter doctor

# Analyze code
flutter analyze

# Run tests
flutter test

# Run on device/emulator
flutter run
```

## Next Steps

1. Configure your backend API URL in `lib/config/app_config.dart`
2. Test the authentication with your backend
3. Customize the app theme and branding
4. Add your app icons and splash screen

## Troubleshooting

### "No pubspec.yaml found"
- Make sure you're in the project root directory
- Verify `pubspec.yaml` exists

### "Flutter SDK not found"
- Install Flutter: https://flutter.dev/docs/get-started/install
- Add Flutter to your PATH

### Platform-specific errors
- Run `flutter create .` as described in Option 1
- This resolves most platform configuration issues

### Dependencies not resolving
```bash
flutter clean
flutter pub get
```

## Platform Requirements

### Android
- Android Studio or Android SDK
- Minimum SDK: 21 (Android 5.0)
- Target SDK: 33 or higher

### iOS
- macOS with Xcode
- iOS 11.0 or higher
- CocoaPods installed

### Web
- Chrome browser (for debugging)
- Enable web support: `flutter config --enable-web`

## Building for Release

### Android
```bash
# Generate release APK
flutter build apk --release

# Generate App Bundle (recommended for Play Store)
flutter build appbundle --release
```

### iOS
```bash
# Build for iOS
flutter build ios --release

# Then archive in Xcode
```

## Additional Resources

- [Flutter Documentation](https://flutter.dev/docs)
- [Flutter Cookbook](https://flutter.dev/docs/cookbook)
- [Dart Documentation](https://dart.dev/guides)
- [Material Design 3](https://m3.material.io/)

## Support

If you encounter issues:
1. Check the main README.md for API configuration
2. Review Flutter doctor output: `flutter doctor -v`
3. Check the troubleshooting section above
4. Open an issue on GitHub
