import 'dart:math' as math;
import 'package:flutter/material.dart';

/// Custom painter for creating animated water wave effect
class WavePainter extends CustomPainter {
  final double animationValue;
  final double waterLevel; // 0.0 to 1.0
  final Color waveColor;
  final Color backgroundColor;
  final bool isDarkMode;

  WavePainter({
    required this.animationValue,
    required this.waterLevel,
    required this.waveColor,
    required this.backgroundColor,
    required this.isDarkMode,
  });

  @override
  void paint(Canvas canvas, Size size) {
    final width = size.width;
    final height = size.height;

    // Calculate water height (from bottom)
    final waterHeight = height * (1 - waterLevel);

    // Paint for the water
    final waterPaint = Paint()
      ..color = waveColor
      ..style = PaintingStyle.fill;

    // Paint for the background
    final bgPaint = Paint()
      ..color = backgroundColor
      ..style = PaintingStyle.fill;

    // Fill background
    canvas.drawRect(
      Rect.fromLTWH(0, 0, width, height),
      bgPaint,
    );

    // Only draw water if level is above 0
    if (waterLevel > 0) {
      // Create wave path
      final wavePath = Path();

      // Wave parameters
      final waveAmplitude = 8.0; // Height of waves
      final waveFrequency = 2.0; // Number of waves
      final waveOffset = animationValue * 2 * math.pi; // Animation offset

      // Start from bottom left
      wavePath.moveTo(0, height);

      // Draw wave line across the top of water
      for (double x = 0; x <= width; x += 1) {
        final normalizedX = x / width;
        final wave1 = math.sin((normalizedX * waveFrequency * 2 * math.pi) + waveOffset);
        final wave2 = math.sin((normalizedX * waveFrequency * 2 * math.pi * 1.5) - waveOffset);
        final combinedWave = (wave1 + wave2) / 2;

        final y = waterHeight + (combinedWave * waveAmplitude);
        wavePath.lineTo(x, y);
      }

      // Complete the path
      wavePath.lineTo(width, height);
      wavePath.close();

      // Draw the water with wave
      canvas.drawPath(wavePath, waterPaint);

      // Add a subtle gradient overlay for depth effect
      if (waterLevel > 0.1) {
        final gradientPaint = Paint()
          ..shader = LinearGradient(
            begin: Alignment.topCenter,
            end: Alignment.bottomCenter,
            colors: isDarkMode
                ? [
                    waveColor.withOpacity(0.6),
                    waveColor.withOpacity(0.9),
                  ]
                : [
                    waveColor.withOpacity(0.7),
                    waveColor.withOpacity(1.0),
                  ],
          ).createShader(Rect.fromLTWH(0, waterHeight, width, height - waterHeight));

        canvas.drawPath(wavePath, gradientPaint);
      }
    }
  }

  @override
  bool shouldRepaint(WavePainter oldDelegate) {
    return oldDelegate.animationValue != animationValue ||
        oldDelegate.waterLevel != waterLevel ||
        oldDelegate.waveColor != waveColor ||
        oldDelegate.isDarkMode != isDarkMode;
  }
}
