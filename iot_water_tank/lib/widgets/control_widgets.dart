import 'package:flutter/material.dart';
import '../models/control_data.dart';

/// Widget for boolean control (switch)
class BooleanControlWidget extends StatelessWidget {
  final ControlData control;
  final ValueChanged<bool> onChanged;
  final bool isSystem;

  const BooleanControlWidget({
    Key? key,
    required this.control,
    required this.onChanged,
    this.isSystem = false,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    final colorScheme = Theme.of(context).colorScheme;

    return Card(
      margin: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Row(
          children: [
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(
                    children: [
                      Text(
                        _formatKey(control.key),
                        style: Theme.of(context).textTheme.titleMedium,
                      ),
                      if (isSystem) ...[
                        const SizedBox(width: 8),
                        Container(
                          padding: const EdgeInsets.symmetric(
                            horizontal: 6,
                            vertical: 2,
                          ),
                          decoration: BoxDecoration(
                            color: colorScheme.tertiaryContainer,
                            borderRadius: BorderRadius.circular(4),
                          ),
                          child: Text(
                            'SYSTEM',
                            style: Theme.of(context).textTheme.labelSmall?.copyWith(
                                  color: colorScheme.onTertiaryContainer,
                                ),
                          ),
                        ),
                      ],
                    ],
                  ),
                  const SizedBox(height: 4),
                  Text(
                    control.boolValue ? 'ON' : 'OFF',
                    style: Theme.of(context).textTheme.bodySmall?.copyWith(
                          color: control.boolValue ? Colors.green : Colors.grey,
                          fontWeight: FontWeight.bold,
                        ),
                  ),
                ],
              ),
            ),
            Switch(
              value: control.boolValue,
              onChanged: onChanged,
            ),
          ],
        ),
      ),
    );
  }

  String _formatKey(String key) {
    // Convert camelCase or snake_case to Title Case
    return key
        .replaceAllMapped(
          RegExp(r'([A-Z])'),
          (match) => ' ${match.group(0)}',
        )
        .replaceAll('_', ' ')
        .trim()
        .split(' ')
        .map((word) => word.isNotEmpty
            ? '${word[0].toUpperCase()}${word.substring(1)}'
            : '')
        .join(' ');
  }
}

/// Widget for number control (slider)
class NumberControlWidget extends StatefulWidget {
  final ControlData control;
  final ValueChanged<double> onChanged;
  final double min;
  final double max;

  const NumberControlWidget({
    Key? key,
    required this.control,
    required this.onChanged,
    this.min = 0,
    this.max = 100,
  }) : super(key: key);

  @override
  State<NumberControlWidget> createState() => _NumberControlWidgetState();
}

class _NumberControlWidgetState extends State<NumberControlWidget> {
  late double _currentValue;

  @override
  void initState() {
    super.initState();
    _currentValue = widget.control.numberValue;
  }

  @override
  void didUpdateWidget(NumberControlWidget oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.control.value != widget.control.value) {
      _currentValue = widget.control.numberValue;
    }
  }

  @override
  Widget build(BuildContext context) {
    return Card(
      margin: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Text(
                  _formatKey(widget.control.key),
                  style: Theme.of(context).textTheme.titleMedium,
                ),
                Container(
                  padding: const EdgeInsets.symmetric(
                    horizontal: 12,
                    vertical: 6,
                  ),
                  decoration: BoxDecoration(
                    color: Theme.of(context).colorScheme.primaryContainer,
                    borderRadius: BorderRadius.circular(8),
                  ),
                  child: Text(
                    _currentValue.toStringAsFixed(1),
                    style: Theme.of(context).textTheme.titleMedium?.copyWith(
                          color: Theme.of(context).colorScheme.onPrimaryContainer,
                          fontWeight: FontWeight.bold,
                        ),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 8),
            Row(
              children: [
                Text(
                  widget.min.toStringAsFixed(0),
                  style: Theme.of(context).textTheme.bodySmall,
                ),
                Expanded(
                  child: Slider(
                    value: _currentValue.clamp(widget.min, widget.max),
                    min: widget.min,
                    max: widget.max,
                    divisions: ((widget.max - widget.min) * 10).toInt(),
                    onChanged: (value) {
                      setState(() {
                        _currentValue = value;
                      });
                    },
                    onChangeEnd: widget.onChanged,
                  ),
                ),
                Text(
                  widget.max.toStringAsFixed(0),
                  style: Theme.of(context).textTheme.bodySmall,
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  String _formatKey(String key) {
    return key
        .replaceAllMapped(
          RegExp(r'([A-Z])'),
          (match) => ' ${match.group(0)}',
        )
        .replaceAll('_', ' ')
        .trim()
        .split(' ')
        .map((word) => word.isNotEmpty
            ? '${word[0].toUpperCase()}${word.substring(1)}'
            : '')
        .join(' ');
  }
}
