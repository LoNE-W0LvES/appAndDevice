import 'package:flutter/material.dart';

/// Combined threshold range card with animated water level indicator
class ThresholdRangeCard extends StatelessWidget {
  final double waterLevel; // 0-100
  final double lowerThreshold; // 0-100
  final double upperThreshold; // 0-100
  final bool isDarkMode;

  const ThresholdRangeCard({
    Key? key,
    required this.waterLevel,
    required this.lowerThreshold,
    required this.upperThreshold,
    required this.isDarkMode,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    // Theme-aware colors
    final cardColor = isDarkMode ? const Color(0xFF1F2937) : const Color(0xFFFFFFFF);
    final borderColor = isDarkMode ? const Color(0xFF374151) : const Color(0xFFE5E7EB);
    final titleColor = isDarkMode ? const Color(0xFF9CA3AF) : const Color(0xFF6B7280);
    final labelColor = isDarkMode ? const Color(0xFF6B7280) : const Color(0xFF9CA3AF);

    return Container(
      padding: const EdgeInsets.all(20),
      decoration: BoxDecoration(
        color: cardColor,
        borderRadius: BorderRadius.circular(20),
        border: Border.all(color: borderColor, width: 1),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Title
          Text(
            'Threshold Range',
            style: TextStyle(
              fontSize: 12,
              color: titleColor,
              fontWeight: FontWeight.w500,
            ),
          ),
          const SizedBox(height: 20),

          // Threshold display with bar
          Row(
            children: [
              // Left: Lower Threshold
              _buildThresholdValue(
                'Lower',
                '${lowerThreshold.toInt()}%',
                const Color(0xFFFB923C), // orange-400
                labelColor,
              ),
              const SizedBox(width: 16),

              // Center: Bar indicator
              Expanded(
                child: _buildBarIndicator(),
              ),
              const SizedBox(width: 16),

              // Right: Upper Threshold
              _buildThresholdValue(
                'Upper',
                '${upperThreshold.toInt()}%',
                const Color(0xFFF87171), // red-400
                labelColor,
              ),
            ],
          ),
        ],
      ),
    );
  }

  /// Build threshold value display (label + value)
  Widget _buildThresholdValue(
    String label,
    String value,
    Color valueColor,
    Color labelColor,
  ) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.center,
      children: [
        Text(
          label,
          style: TextStyle(
            fontSize: 10,
            color: labelColor,
            fontWeight: FontWeight.w500,
          ),
        ),
        const SizedBox(height: 4),
        Text(
          value,
          style: TextStyle(
            fontSize: 20,
            fontWeight: FontWeight.bold,
            color: valueColor,
          ),
        ),
      ],
    );
  }

  /// Build animated bar indicator with water level dot
  Widget _buildBarIndicator() {
    const barHeight = 1.5;
    const dotSize = 12.0;
    const grayColor = Color(0xFF374151); // gray-700
    const cyanColor = Color(0xFF06B6D4); // cyan-500
    const dotColor = Colors.white;
    const dotBorderColor = Color(0xFF22D3EE); // cyan-400

    return LayoutBuilder(
      builder: (context, constraints) {
        final barWidth = constraints.maxWidth;

        // Calculate positions
        final lowerPos = (lowerThreshold / 100) * barWidth;
        final upperPos = (upperThreshold / 100) * barWidth;
        final waterPos = (waterLevel / 100) * barWidth;

        return SizedBox(
          height: 40, // Height to accommodate dot
          child: Stack(
            alignment: Alignment.center,
            clipBehavior: Clip.none,
            children: [
              // Background bar
              Positioned(
                left: 0,
                right: 0,
                child: Container(
                  height: barHeight,
                  decoration: BoxDecoration(
                    color: grayColor.withOpacity(0.3),
                    borderRadius: BorderRadius.circular(barHeight / 2),
                  ),
                ),
              ),

              // Colored segments
              // Segment 1: 0% to lower threshold (gray)
              if (lowerThreshold > 0)
                Positioned(
                  left: 0,
                  width: lowerPos,
                  child: Container(
                    height: barHeight,
                    decoration: BoxDecoration(
                      color: grayColor,
                      borderRadius: BorderRadius.circular(barHeight / 2),
                    ),
                  ),
                ),

              // Segment 2: lower to upper threshold (cyan)
              if (upperThreshold > lowerThreshold)
                Positioned(
                  left: lowerPos,
                  width: upperPos - lowerPos,
                  child: Container(
                    height: barHeight,
                    decoration: BoxDecoration(
                      color: cyanColor,
                      borderRadius: BorderRadius.circular(barHeight / 2),
                    ),
                  ),
                ),

              // Segment 3: upper threshold to 100% (gray)
              if (upperThreshold < 100)
                Positioned(
                  left: upperPos,
                  right: 0,
                  child: Container(
                    height: barHeight,
                    decoration: BoxDecoration(
                      color: grayColor,
                      borderRadius: BorderRadius.circular(barHeight / 2),
                    ),
                  ),
                ),

              // Animated water level dot
              AnimatedPositioned(
                duration: const Duration(seconds: 1),
                curve: Curves.easeInOut,
                left: waterPos.clamp(0.0, barWidth) - (dotSize / 2),
                child: Container(
                  width: dotSize,
                  height: dotSize,
                  decoration: BoxDecoration(
                    color: dotColor,
                    shape: BoxShape.circle,
                    border: Border.all(
                      color: dotBorderColor,
                      width: 2,
                    ),
                    boxShadow: [
                      BoxShadow(
                        color: dotBorderColor.withOpacity(0.6),
                        blurRadius: 8,
                        spreadRadius: 2,
                      ),
                      BoxShadow(
                        color: dotBorderColor.withOpacity(0.3),
                        blurRadius: 16,
                        spreadRadius: 4,
                      ),
                    ],
                  ),
                ),
              ),

              // Threshold markers (small vertical lines)
              if (lowerThreshold > 0)
                Positioned(
                  left: lowerPos,
                  child: Container(
                    width: 2,
                    height: 8,
                    decoration: BoxDecoration(
                      color: const Color(0xFFFB923C).withOpacity(0.5),
                      borderRadius: BorderRadius.circular(1),
                    ),
                  ),
                ),
              if (upperThreshold < 100)
                Positioned(
                  left: upperPos,
                  child: Container(
                    width: 2,
                    height: 8,
                    decoration: BoxDecoration(
                      color: const Color(0xFFF87171).withOpacity(0.5),
                      borderRadius: BorderRadius.circular(1),
                    ),
                  ),
                ),
            ],
          ),
        );
      },
    );
  }
}
