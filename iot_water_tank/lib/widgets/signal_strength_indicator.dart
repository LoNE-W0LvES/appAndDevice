import 'package:flutter/material.dart';

class SignalStrengthIndicator extends StatelessWidget {
  final int bars; // 1-4 bars
  final int signalStrength; // 0-100 percentage
  final Color? color;
  final double size;

  const SignalStrengthIndicator({
    Key? key,
    required this.bars,
    required this.signalStrength,
    this.color,
    this.size = 20,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    final effectiveColor = color ?? _getColorForSignalStrength(signalStrength);

    return SizedBox(
      width: size,
      height: size,
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        crossAxisAlignment: CrossAxisAlignment.end,
        children: List.generate(4, (index) {
          final isActive = index < bars;
          final barHeight = (size / 4) * (index + 1);

          return Container(
            width: size / 6,
            height: barHeight,
            decoration: BoxDecoration(
              color: isActive ? effectiveColor : effectiveColor.withOpacity(0.2),
              borderRadius: BorderRadius.circular(1),
            ),
          );
        }),
      ),
    );
  }

  /// Get color based on signal strength
  Color _getColorForSignalStrength(int strength) {
    if (strength >= 75) {
      return const Color(0xFF22D3EE); // Cyan-400 (strong)
    } else if (strength >= 50) {
      return const Color(0xFF06B6D4); // Cyan-500 (good)
    } else if (strength >= 25) {
      return const Color(0xFF9CA3AF); // Gray-400 (fair)
    } else {
      return const Color(0xFF6B7280); // Gray-500 (poor)
    }
  }
}
