import 'package:flutter/material.dart';
import '../models/wifi_network.dart';
import 'signal_strength_indicator.dart';

class NetworkListTile extends StatelessWidget {
  final WiFiNetwork network;
  final bool isSelected;
  final VoidCallback onTap;

  const NetworkListTile({
    Key? key,
    required this.network,
    required this.isSelected,
    required this.onTap,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final isDark = theme.brightness == Brightness.dark;

    return Container(
      margin: const EdgeInsets.only(bottom: 8),
      decoration: BoxDecoration(
        color: isSelected
            ? theme.primaryColor.withOpacity(0.1)
            : (isDark ? const Color(0xFF1F2937) : Colors.white),
        border: Border.all(
          color: isSelected
              ? theme.primaryColor
              : (isDark ? const Color(0xFF374151) : const Color(0xFFE5E7EB)),
          width: isSelected ? 2 : 1,
        ),
        borderRadius: BorderRadius.circular(8),
      ),
      child: ListTile(
        onTap: onTap,
        leading: Icon(
          network.isSecured ? Icons.lock : Icons.lock_open,
          color: network.isSecured
              ? (isDark ? const Color(0xFF9CA3AF) : const Color(0xFF6B7280))
              : theme.primaryColor,
        ),
        title: Text(
          network.ssid,
          style: theme.textTheme.bodyLarge?.copyWith(
            fontWeight: isSelected ? FontWeight.w600 : FontWeight.w500,
            color: isDark ? Colors.white : const Color(0xFF111827),
          ),
        ),
        subtitle: Row(
          children: [
            Text(
              network.auth,
              style: theme.textTheme.bodySmall?.copyWith(
                color: isDark
                    ? const Color(0xFF9CA3AF)
                    : const Color(0xFF6B7280),
              ),
            ),
            const SizedBox(width: 8),
            Text(
              '• ${network.signalQuality}',
              style: theme.textTheme.bodySmall?.copyWith(
                color: isDark
                    ? const Color(0xFF9CA3AF)
                    : const Color(0xFF6B7280),
              ),
            ),
          ],
        ),
        trailing: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            SignalStrengthIndicator(
              bars: network.bars,
              signalStrength: network.signalStrength,
            ),
            const SizedBox(height: 4),
            Text(
              '${network.signalStrength}%',
              style: theme.textTheme.bodySmall?.copyWith(
                fontSize: 10,
                color: isDark
                    ? const Color(0xFF9CA3AF)
                    : const Color(0xFF6B7280),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
