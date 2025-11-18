/// Custom exception for API errors
class ApiException implements Exception {
  final String message;
  final int? statusCode;
  final dynamic data;

  ApiException({
    required this.message,
    this.statusCode,
    this.data,
  });

  @override
  String toString() => message;

  /// Create from DioError or other error
  factory ApiException.fromError(dynamic error) {
    if (error is ApiException) return error;

    // Handle different error types
    String message = 'An unexpected error occurred';
    int? statusCode;
    dynamic data;

    try {
      // Try to extract error information
      if (error.response != null) {
        statusCode = error.response.statusCode;
        data = error.response.data;

        if (data is Map && data.containsKey('message')) {
          message = data['message'].toString();
        } else if (data is Map && data.containsKey('error')) {
          message = data['error'].toString();
        } else if (statusCode == 401) {
          message = 'Unauthorized. Please login again.';
        } else if (statusCode == 404) {
          message = 'Resource not found';
        } else if (statusCode == 500) {
          message = 'Server error. Please try again later.';
        } else {
          message = 'Request failed with status code $statusCode';
        }
      } else if (error.message != null) {
        message = error.message.toString();
      }
    } catch (e) {
      message = 'Failed to parse error: ${error.toString()}';
    }

    return ApiException(
      message: message,
      statusCode: statusCode,
      data: data,
    );
  }
}
