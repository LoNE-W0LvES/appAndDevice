#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <Arduino.h>
#include "config.h"

// Button event types
enum ButtonEvent {
    BTN_NONE,
    BTN1_PRESSED,      // Cycle display screens
    BTN2_PRESSED,      // Manual pump ON
    BTN3_PRESSED,      // Toggle Auto/Manual mode
    BTN4_PRESSED,      // Manual pump OFF
    BTN5_SHORT_PRESS,  // (Reserved for future use)
    BTN5_LONG_PRESS,   // Reset WiFi
    BTN6_PRESSED       // Hardware override toggle
};

struct Button {
    uint8_t pin;
    bool lastState;
    bool currentState;
    unsigned long pressTime;
    bool longPressTriggered;
};

class ButtonHandler {
public:
    ButtonHandler();

    // Initialize button pins
    void begin();

    // Update button states (call in loop)
    void update();

    // Get button event (non-blocking)
    ButtonEvent getEvent();

    // Check if specific button is pressed (for continuous reading)
    bool isButtonPressed(uint8_t buttonNum);

private:
    Button buttons[6];

    // Debounce and detect button presses
    void readButton(int index);

    // Check for long press on BTN5
    void checkLongPress();

    ButtonEvent pendingEvent;
};

#endif // BUTTON_HANDLER_H
