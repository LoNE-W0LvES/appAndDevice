# IoT Water Tank - Flutter Mobile App

A comprehensive Flutter mobile application for managing and controlling IoT devices with real-time monitoring and control capabilities.

## Features

### Authentication
- Email/password login with Better Auth
- Session persistence with cookies
- Automatic session management
- Remember login state

### Device Management
- View all assigned devices
- Filter devices by project
- Real-time device status monitoring
- Pull-to-refresh functionality
- Device search and filtering

### Device Control
- Interactive control widgets:
  - Boolean controls → Toggle switches
  - Number controls → Sliders with value display
- Real-time telemetry data display
- System flags management (force_update, config_update)
- Instant control updates with feedback

### UI/UX
- Material Design 3 theming
- Light and dark mode support
- Responsive design
- Loading states and error handling
- Offline mode detection

## Architecture

### Project Structure
```
lib/
├── config/          # App configuration
│   └── app_config.dart
├── models/          # Data models
│   ├── device.dart
│   ├── control_data.dart
│   └── telemetry_data.dart
├── services/        # API and business logic
│   ├── api_client.dart
│   ├── auth_service.dart
│   └── device_service.dart
├── providers/       # State management (Provider pattern)
│   ├── auth_provider.dart
│   └── device_provider.dart
├── screens/         # UI screens
│   ├── login_screen.dart
│   ├── device_list_screen.dart
│   └── device_control_screen.dart
├── widgets/         # Reusable widgets
│   ├── loading_widget.dart
│   ├── error_widget.dart
│   ├── device_card.dart
│   └── control_widgets.dart
├── utils/           # Utilities
│   └── api_exception.dart
└── main.dart        # App entry point
```

### State Management
- **Provider** for state management
- Separate providers for authentication and device management
- Reactive UI updates

### Networking
- **Dio** for HTTP requests
- Cookie-based session management
- Automatic error handling and retry logic
- Request/response logging in debug mode

## Configuration

### API Configuration
Edit `lib/config/app_config.dart` to customize for your project:

```dart
class AppConfig {
  // API Configuration
  static const String baseUrl = 'http://103.136.236.16';
  static const String apiPrefix = '/api';

  // App Configuration
  static const String appName = 'IoT Water Tank';

  // Timeouts
  static const int connectionTimeout = 30;
  static const int receiveTimeout = 30;

  // System Control Keys
  static const List<String> systemControlKeys = [
    'force_update',
    'config_update',
  ];
}
```

### Backend API Requirements

The app expects the following API endpoints:

1. **Authentication**
   - POST `/api/auth/sign-in/email`
     - Body: `{ "email": "user@example.com", "password": "password" }`
     - Response: Sets `better-auth.session_token` cookie

2. **Devices**
   - GET `/api/devices?assignedOnly=true`
     - Returns: `{ "data": [Device[], ...] }`

   - GET `/api/devices/{id}`
     - Returns: `{ "data": Device }`

   - PATCH `/api/devices/{id}`
     - Body: `{ "controlData": { "key": { "key": "...", "type": "...", "value": ... } } }`
     - Returns: Updated device

### Device Data Structure

```json
{
  "id": "device-id",
  "name": "Device Name",
  "deviceId": "unique-device-id",
  "isActive": true,
  "lastSeen": "2024-01-01T00:00:00Z",
  "projectId": "project-id",
  "projectName": "Project Name",
  "controlData": {
    "pumpSwitch": {
      "type": "boolean",
      "value": true
    },
    "temperature": {
      "type": "number",
      "value": 25.5
    }
  },
  "telemetryData": {
    "waterLevel": {
      "type": "number",
      "value": 75.2
    }
  }
}
```

## Setup Instructions

### Prerequisites
- Flutter SDK (3.0.0 or higher)
- Dart SDK (3.0.0 or higher)
- Android Studio / Xcode (for mobile development)
- VS Code (recommended)

### Installation

1. **Clone the repository**
   ```bash
   git clone <repository-url>
   cd IOTWaterTank
   ```

2. **Install dependencies**
   ```bash
   flutter pub get
   ```

3. **Configure your API**
   - Edit `lib/config/app_config.dart`
   - Update `baseUrl` to your backend server
   - Customize app name and other settings

4. **Run the app**
   ```bash
   # Run on connected device/emulator
   flutter run

   # Run in debug mode
   flutter run --debug

   # Run in release mode
   flutter run --release
   ```

### Building for Production

#### Android
```bash
# Build APK
flutter build apk --release

# Build App Bundle (for Google Play)
flutter build appbundle --release
```

#### iOS
```bash
# Build iOS app
flutter build ios --release
```

## Development

### Running Tests
```bash
flutter test
```

### Code Analysis
```bash
flutter analyze
```

### Format Code
```bash
flutter format lib/
```

## Customization Guide

### 1. Change App Name
Edit `lib/config/app_config.dart`:
```dart
static const String appName = 'Your App Name';
```

### 2. Change Theme Colors
Edit `lib/main.dart` in the `_buildLightTheme()` and `_buildDarkTheme()` methods:
```dart
colorScheme: ColorScheme.fromSeed(
  seedColor: Colors.yourColor,  // Change this
  brightness: Brightness.light,
),
```

### 3. Add Custom Control Types
1. Create a new widget in `lib/widgets/control_widgets.dart`
2. Add the widget to `device_control_screen.dart`
3. Update the control type handling logic

### 4. Customize API Endpoints
Edit `lib/config/app_config.dart`:
```dart
static const String signInEndpoint = '$apiPrefix/your/auth/endpoint';
static const String devicesEndpoint = '$apiPrefix/your/devices/endpoint';
```

## Troubleshooting

### Common Issues

1. **Connection refused / Network error**
   - Check if the backend server is running
   - Verify the API URL in `app_config.dart`
   - Check network permissions in Android/iOS

2. **Cookie not persisting**
   - Ensure backend is setting cookies correctly
   - Check cookie name matches in config
   - Verify cookie domain settings

3. **Build errors**
   - Run `flutter clean`
   - Delete `pubspec.lock`
   - Run `flutter pub get`

4. **Dependencies issues**
   - Update Flutter: `flutter upgrade`
   - Update dependencies: `flutter pub upgrade`

## API Integration Examples

### Custom Device Service
```dart
// Extend DeviceService for custom endpoints
class CustomDeviceService extends DeviceService {
  Future<void> customAction(String deviceId) async {
    await _apiClient.post('/api/devices/$deviceId/custom-action');
  }
}
```

### Adding New Providers
```dart
// Create a new provider
class CustomProvider with ChangeNotifier {
  // Your state and methods
}

// Register in main.dart
MultiProvider(
  providers: [
    ChangeNotifierProvider(create: (_) => CustomProvider()),
    // ... other providers
  ],
)
```

## Security Notes

- Session tokens are stored securely using platform-specific storage
- HTTPS is recommended for production
- Implement proper backend authentication and authorization
- Validate all user inputs on the backend
- Use environment variables for sensitive configuration

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## License

This project is licensed under the MIT License.

## Support

For issues and questions:
- Open an issue on GitHub
- Contact the development team

## Roadmap

- [ ] Push notifications for device alerts
- [ ] Device grouping and bulk controls
- [ ] Historical data visualization
- [ ] Export telemetry data
- [ ] Multi-language support
- [ ] Biometric authentication
- [ ] Offline mode with data sync

## Credits

Built with Flutter and Material Design 3.
