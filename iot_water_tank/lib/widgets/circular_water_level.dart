import 'package:flutter/material.dart';
import 'dart:math' as math;
import 'wave_painter.dart';

/// Circular water level indicator with animated wave effect
class CircularWaterLevel extends StatefulWidget {
  final double percentage; // 0.0 to 100.0
  final double size;
  final bool isDarkMode;
  final bool isPumpOn;
  final double lowerThreshold;
  final double upperThreshold;

  const CircularWaterLevel({
    Key? key,
    required this.percentage,
    this.size = 280,
    required this.isDarkMode,
    required this.isPumpOn,
    required this.lowerThreshold,
    required this.upperThreshold,
  }) : super(key: key);

  @override
  State<CircularWaterLevel> createState() => _CircularWaterLevelState();
}

class _CircularWaterLevelState extends State<CircularWaterLevel>
    with SingleTickerProviderStateMixin {
  late AnimationController _waveController;
  late Animation<double> _waveAnimation;

  @override
  void initState() {
    super.initState();

    // Initialize wave animation
    _waveController = AnimationController(
      duration: const Duration(seconds: 3),
      vsync: this,
    )..repeat();

    _waveAnimation = Tween<double>(begin: 0.0, end: 1.0).animate(_waveController);
  }

  @override
  void dispose() {
    _waveController.dispose();
    super.dispose();
  }

  /// Calculate water level color based on pump status and water level
  Color _getWaterLevelColor(double percentage) {
    // If pump is ON, always return green
    if (widget.isPumpOn) {
      return Colors.green;
    }

    // Cyan color (ocean blue/sky blue)
    const cyanColor = Color(0xFF22D3EE); // cyan-400

    // If pump is OFF, gradient from red (low) to cyan (high)
    // Calculate position between lower and upper thresholds
    final lower = widget.lowerThreshold;
    final upper = widget.upperThreshold;

    // If below lower threshold, return red
    if (percentage <= lower) {
      return Colors.red;
    }

    // If above upper threshold, return cyan
    if (percentage >= upper) {
      return cyanColor;
    }

    // Between thresholds: interpolate from red to cyan
    final range = upper - lower;
    final position = (percentage - lower) / range; // 0.0 to 1.0

    // Interpolate between red and cyan
    return Color.lerp(Colors.red, cyanColor, position)!;
  }

  @override
  Widget build(BuildContext context) {
    final colorScheme = Theme.of(context).colorScheme;
    final textTheme = Theme.of(context).textTheme;

    // Clamp percentage between 0 and 100
    final displayPercentage = widget.percentage.clamp(0.0, 100.0);
    final waterLevel = displayPercentage / 100.0;

    // Dynamic color based on pump status and water level
    final primaryColor = _getWaterLevelColor(displayPercentage);

    // Theme-aware colors
    final surfaceColor = widget.isDarkMode
        ? const Color(0xFF1F2937) // dark gray-800
        : const Color(0xFFF9FAFB); // light gray-50
    final borderColor = widget.isDarkMode
        ? const Color(0xFF374151) // dark gray-700
        : const Color(0xFFE5E7EB); // light gray-200

    // Add extra space for glow effect (blurRadius + spreadRadius needs room)
    final containerSize = widget.size + 100; // Extra 100px for full glow visibility

    return SizedBox(
      width: containerSize,
      height: containerSize,
      child: Stack(
        alignment: Alignment.center,
        clipBehavior: Clip.none, // Allow glow to extend beyond bounds
        children: [
          // Outer glow effect (only for dark mode)
          if (widget.isDarkMode)
            Container(
              width: widget.size + 20,
              height: widget.size + 20,
              decoration: BoxDecoration(
                shape: BoxShape.circle,
                boxShadow: [
                  BoxShadow(
                    color: primaryColor.withOpacity(0.3),
                    blurRadius: 40,
                    spreadRadius: 10,
                  ),
                ],
              ),
            ),

          // Main circle with border
          Container(
            width: widget.size,
            height: widget.size,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              border: Border.all(
                color: borderColor,
                width: 4,
              ),
              color: surfaceColor,
            ),
          ),

          // Circular progress indicator
          SizedBox(
            width: widget.size - 8,
            height: widget.size - 8,
            child: CircularProgressIndicator(
              value: waterLevel,
              strokeWidth: 8,
              backgroundColor: borderColor.withOpacity(0.3),
              valueColor: AlwaysStoppedAnimation<Color>(primaryColor),
            ),
          ),

          // Water with wave animation (clipped to circle)
          ClipOval(
            child: SizedBox(
              width: widget.size - 16,
              height: widget.size - 16,
              child: AnimatedBuilder(
                animation: _waveAnimation,
                builder: (context, child) {
                  return CustomPaint(
                    painter: WavePainter(
                      animationValue: _waveAnimation.value,
                      waterLevel: waterLevel,
                      waveColor: primaryColor.withOpacity(0.4),
                      backgroundColor: Colors.transparent,
                      isDarkMode: widget.isDarkMode,
                    ),
                  );
                },
              ),
            ),
          ),

          // Center text
          Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              // Pump Status (ON/OFF)
              Container(
                padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 4),
                decoration: BoxDecoration(
                  color: primaryColor.withOpacity(0.2),
                  borderRadius: BorderRadius.circular(12),
                  border: Border.all(
                    color: primaryColor,
                    width: 2,
                  ),
                ),
                child: Text(
                  widget.isPumpOn ? 'ON' : 'OFF',
                  style: TextStyle(
                    fontSize: widget.size * 0.05,
                    fontWeight: FontWeight.bold,
                    color: primaryColor,
                    letterSpacing: 2,
                  ),
                ),
              ),
              const SizedBox(height: 12),
              // Percentage
              Text(
                '${displayPercentage.toInt()}%',
                style: textTheme.displayLarge?.copyWith(
                  fontSize: widget.size * 0.22,
                  fontWeight: FontWeight.bold,
                  color: colorScheme.onSurface,
                  shadows: widget.isDarkMode
                      ? [
                          Shadow(
                            color: primaryColor.withOpacity(0.5),
                            blurRadius: 20,
                          ),
                        ]
                      : null,
                ),
              ),
              const SizedBox(height: 4),
              // Label
              Text(
                'CURRENT LEVEL',
                style: textTheme.labelLarge?.copyWith(
                  fontSize: widget.size * 0.04,
                  letterSpacing: 1.5,
                  fontWeight: FontWeight.w600,
                  color: colorScheme.onSurface.withOpacity(0.7),
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }
}
