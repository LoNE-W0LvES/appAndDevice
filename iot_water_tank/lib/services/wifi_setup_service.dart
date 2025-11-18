import 'package:dio/dio.dart';
import '../models/wifi_network.dart';
import '../utils/api_exception.dart';

/// Service for WiFi setup via local device connection (192.168.4.1)
class WiFiSetupService {
  static const String _baseUrl = 'http://192.168.4.1';
  static const Duration _timeout = Duration(seconds: 5);
  static const Duration _scanTimeout = Duration(seconds: 30); // Longer timeout for WiFi scan

  late final Dio _dio;

  WiFiSetupService() {
    _dio = Dio(BaseOptions(
      baseUrl: _baseUrl,
      connectTimeout: _timeout,
      receiveTimeout: _timeout,
      sendTimeout: _timeout,
      validateStatus: (status) => status != null && status < 500,
    ));

    // Add logging interceptor for debugging
    _dio.interceptors.add(LogInterceptor(
      requestBody: true,
      responseBody: true,
      error: true,
      logPrint: (obj) => print('[WiFiSetup] $obj'),
    ));
  }

  /// Check if device is ready for WiFi setup
  /// GET http://192.168.4.1/{device_id}/status
  Future<DeviceStatusResponse> checkDeviceStatus(String deviceId) async {
    try {
      final response = await _dio.get('/$deviceId/status');

      if (response.statusCode == 200) {
        return DeviceStatusResponse.fromJson(response.data);
      } else {
        throw ApiException(
          message: 'Failed to check device status',
          statusCode: response.statusCode,
        );
      }
    } on DioException catch (e) {
      throw _handleDioException(e);
    }
  }

  /// Scan for available WiFi networks
  /// GET http://192.168.4.1/{device_id}/scanWifi
  /// Note: WiFi scanning can take 15-30 seconds, especially with concurrent sensor readings
  Future<WiFiScanResponse> scanWiFiNetworks(String deviceId) async {
    try {
      final response = await _dio.get(
        '/$deviceId/scanWifi',
        options: Options(
          receiveTimeout: _scanTimeout,
          sendTimeout: _scanTimeout,
        ),
      );

      if (response.statusCode == 200) {
        return WiFiScanResponse.fromJson(response.data);
      } else {
        throw ApiException(
          message: 'Failed to scan WiFi networks',
          statusCode: response.statusCode,
        );
      }
    } on DioException catch (e) {
      throw _handleDioException(e);
    }
  }

  /// Save WiFi credentials to device
  /// POST http://192.168.4.1/{device_id}/save
  Future<WiFiSetupResponse> saveWiFiCredentials({
    required String deviceId,
    required String ssid,
    required String password,
    String? dashboardUsername,
    String? dashboardPassword,
  }) async {
    try {
      final data = {
        'ssid': ssid,
        'password': password,
      };

      // Add dashboard credentials if provided
      if (dashboardUsername != null && dashboardUsername.isNotEmpty) {
        data['dashboardUsername'] = dashboardUsername;
      }
      if (dashboardPassword != null && dashboardPassword.isNotEmpty) {
        data['dashboardPassword'] = dashboardPassword;
      }

      final response = await _dio.post(
        '/$deviceId/save',
        data: data,
      );

      if (response.statusCode == 200) {
        return WiFiSetupResponse.fromJson(response.data);
      } else {
        throw ApiException(
          message: 'Failed to save WiFi credentials',
          statusCode: response.statusCode,
        );
      }
    } on DioException catch (e) {
      throw _handleDioException(e);
    }
  }

  /// Handle Dio exceptions and convert to ApiException
  ApiException _handleDioException(DioException e) {
    switch (e.type) {
      case DioExceptionType.connectionTimeout:
      case DioExceptionType.sendTimeout:
      case DioExceptionType.receiveTimeout:
        return ApiException(
          message: 'Connection timeout. Please check if you are connected to the device hotspot.',
          statusCode: 408,
        );
      case DioExceptionType.connectionError:
        return ApiException(
          message: 'Cannot connect to device. Please ensure you are connected to the device hotspot (AquaFlow-XXXXX).',
          statusCode: 0,
        );
      case DioExceptionType.badResponse:
        return ApiException(
          message: e.response?.data?['message'] ?? 'Bad response from device',
          statusCode: e.response?.statusCode,
        );
      default:
        return ApiException(
          message: e.message ?? 'Unknown error occurred',
          statusCode: 0,
        );
    }
  }

  /// Dispose resources
  void dispose() {
    _dio.close();
  }
}
