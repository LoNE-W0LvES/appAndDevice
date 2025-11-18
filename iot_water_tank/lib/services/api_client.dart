import 'package:dio/dio.dart';
import 'package:cookie_jar/cookie_jar.dart';
import 'package:dio_cookie_manager/dio_cookie_manager.dart';
import 'package:path_provider/path_provider.dart';
import 'dart:io';
import '../config/app_config.dart';
import '../utils/api_exception.dart';

/// API client with cookie management for session persistence
class ApiClient {
  static final ApiClient _instance = ApiClient._internal();
  factory ApiClient() => _instance;

  late Dio _dio;
  late CookieJar _cookieJar;
  bool _initialized = false;

  ApiClient._internal();

  /// Initialize the API client
  Future<void> initialize() async {
    if (_initialized) return;

    // Use PersistCookieJar to save cookies to disk
    final Directory appDocDir = await getApplicationDocumentsDirectory();
    final String appDocPath = appDocDir.path;
    _cookieJar = PersistCookieJar(
      storage: FileStorage('$appDocPath/.cookies/'),
    );

    _dio = Dio(BaseOptions(
      baseUrl: AppConfig.baseUrl,
      connectTimeout: Duration(seconds: AppConfig.connectionTimeout),
      receiveTimeout: Duration(seconds: AppConfig.receiveTimeout),
      headers: {
        'Content-Type': 'application/json',
        'Accept': 'application/json',
      },
    ));

    // Add cookie manager
    _dio.interceptors.add(CookieManager(_cookieJar));

    // Add logging interceptor in debug mode
    if (AppConfig.enableDebugMode) {
      _dio.interceptors.add(LogInterceptor(
        requestBody: true,
        responseBody: true,
        error: true,
      ));
    }

    // Add response and error handling interceptor
    _dio.interceptors.add(InterceptorsWrapper(
      onRequest: (options, handler) {
        print('[ApiClient] ${options.method} ${options.baseUrl}${options.path}');
        if (options.queryParameters.isNotEmpty) {
          print('[ApiClient] Query Parameters: ${options.queryParameters}');
        }
        if (options.data != null) {
          print('[ApiClient] Request Data: ${options.data}');
        }
        handler.next(options);
      },
      onResponse: (response, handler) {
        print('[ApiClient] Response ${response.statusCode} from ${response.requestOptions.path}');
        print('[ApiClient] Response Data: ${response.data}');
        handler.next(response);
      },
      onError: (error, handler) {
        print('[ApiClient] API Error: ${error.message}');
        if (error.response != null) {
          print('[ApiClient] Error Status Code: ${error.response?.statusCode}');
          print('[ApiClient] Error Response Data: ${error.response?.data}');
        }
        handler.next(error);
      },
    ));

    _initialized = true;
  }

  /// Get Dio instance
  Dio get dio {
    if (!_initialized) {
      throw Exception('ApiClient not initialized. Call initialize() first.');
    }
    return _dio;
  }

  /// GET request
  Future<Response> get(
    String path, {
    Map<String, dynamic>? queryParameters,
    Options? options,
  }) async {
    try {
      return await _dio.get(
        path,
        queryParameters: queryParameters,
        options: options,
      );
    } catch (e) {
      throw ApiException.fromError(e);
    }
  }

  /// POST request
  Future<Response> post(
    String path, {
    dynamic data,
    Map<String, dynamic>? queryParameters,
    Options? options,
  }) async {
    try {
      return await _dio.post(
        path,
        data: data,
        queryParameters: queryParameters,
        options: options,
      );
    } catch (e) {
      throw ApiException.fromError(e);
    }
  }

  /// PATCH request
  Future<Response> patch(
    String path, {
    dynamic data,
    Map<String, dynamic>? queryParameters,
    Options? options,
  }) async {
    try {
      return await _dio.patch(
        path,
        data: data,
        queryParameters: queryParameters,
        options: options,
      );
    } catch (e) {
      throw ApiException.fromError(e);
    }
  }

  /// PUT request
  Future<Response> put(
    String path, {
    dynamic data,
    Map<String, dynamic>? queryParameters,
    Options? options,
  }) async {
    try {
      return await _dio.put(
        path,
        data: data,
        queryParameters: queryParameters,
        options: options,
      );
    } catch (e) {
      throw ApiException.fromError(e);
    }
  }

  /// DELETE request
  Future<Response> delete(
    String path, {
    dynamic data,
    Map<String, dynamic>? queryParameters,
    Options? options,
  }) async {
    try {
      return await _dio.delete(
        path,
        data: data,
        queryParameters: queryParameters,
        options: options,
      );
    } catch (e) {
      throw ApiException.fromError(e);
    }
  }

  /// Clear all cookies
  Future<void> clearCookies() async {
    await _cookieJar.deleteAll();
  }

  /// Get cookies for a specific URI
  Future<List<Cookie>> getCookies(String uri) async {
    return await _cookieJar.loadForRequest(Uri.parse(uri));
  }
}
