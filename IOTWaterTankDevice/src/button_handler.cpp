#include "button_handler.h"

ButtonHandler::ButtonHandler() : pendingEvent(BTN_NONE) {
    // Initialize button structures
    uint8_t buttonPins[] = {BTN1_PIN, BTN2_PIN, BTN3_PIN, BTN4_PIN, BTN5_PIN, BTN6_PIN};

    for (int i = 0; i < 6; i++) {
        buttons[i].pin = buttonPins[i];
        buttons[i].lastState = HIGH;
        buttons[i].currentState = HIGH;
        buttons[i].pressTime = 0;
        buttons[i].longPressTriggered = false;
    }
}

void ButtonHandler::begin() {
    // Initialize all button pins with internal pull-up
    for (int i = 0; i < 6; i++) {
        pinMode(buttons[i].pin, INPUT_PULLUP);
    }

    Serial.println("[Button] Button handler initialized");
    Serial.println("[Button] BTN1 (Pin " + String(BTN1_PIN) + "): Cycle screens");
    Serial.println("[Button] BTN2 (Pin " + String(BTN2_PIN) + "): Pump ON");
    Serial.println("[Button] BTN3 (Pin " + String(BTN3_PIN) + "): Toggle mode");
    Serial.println("[Button] BTN4 (Pin " + String(BTN4_PIN) + "): Pump OFF");
    Serial.println("[Button] BTN5 (Pin " + String(BTN5_PIN) + "): WiFi reset (long)");
    Serial.println("[Button] BTN6 (Pin " + String(BTN6_PIN) + "): Hardware override");
}

void ButtonHandler::readButton(int index) {
    Button& btn = buttons[index];

    // Read current state (LOW = pressed with pull-up)
    bool reading = digitalRead(btn.pin);

    // Debounce logic
    if (reading != btn.currentState) {
        btn.currentState = reading;

        // Button just pressed (HIGH -> LOW transition)
        if (reading == LOW) {
            btn.pressTime = millis();
            btn.longPressTriggered = false;

            // Generate immediate event for buttons 1-4 and 6
            if (index == 0 && pendingEvent == BTN_NONE) {
                pendingEvent = BTN1_PRESSED;
                Serial.println("[Button] BTN1 pressed - Cycle screen");
            } else if (index == 1 && pendingEvent == BTN_NONE) {
                pendingEvent = BTN2_PRESSED;
                Serial.println("[Button] BTN2 pressed - Pump ON");
            } else if (index == 2 && pendingEvent == BTN_NONE) {
                pendingEvent = BTN3_PRESSED;
                Serial.println("[Button] BTN3 pressed - Toggle mode");
            } else if (index == 3 && pendingEvent == BTN_NONE) {
                pendingEvent = BTN4_PRESSED;
                Serial.println("[Button] BTN4 pressed - Pump OFF");
            } else if (index == 5 && pendingEvent == BTN_NONE) {
                pendingEvent = BTN6_PRESSED;
                Serial.println("[Button] BTN6 pressed - Hardware override");
            }
        }
        // Button released (LOW -> HIGH transition)
        else if (reading == HIGH) {
            // For BTN5, check if it was a short press
            if (index == 4 && !btn.longPressTriggered && pendingEvent == BTN_NONE) {
                unsigned long pressDuration = millis() - btn.pressTime;
                if (pressDuration < BUTTON_LONG_PRESS_MS) {
                    pendingEvent = BTN5_SHORT_PRESS;
                    Serial.println("[Button] BTN5 short press");
                }
            }
        }
    }

    btn.lastState = btn.currentState;
}

void ButtonHandler::checkLongPress() {
    // Check BTN5 for long press
    Button& btn5 = buttons[4]; // Index 4 = BTN5

    if (btn5.currentState == LOW && !btn5.longPressTriggered) {
        unsigned long pressDuration = millis() - btn5.pressTime;

        if (pressDuration >= BUTTON_LONG_PRESS_MS) {
            btn5.longPressTriggered = true;

            if (pendingEvent == BTN_NONE) {
                pendingEvent = BTN5_LONG_PRESS;
                Serial.println("[Button] BTN5 long press - WiFi reset");
            }
        }
    }
}

void ButtonHandler::update() {
    // Read all buttons
    for (int i = 0; i < 6; i++) {
        readButton(i);
    }

    // Check for long press on BTN5
    checkLongPress();
}

ButtonEvent ButtonHandler::getEvent() {
    ButtonEvent event = pendingEvent;
    pendingEvent = BTN_NONE; // Clear event after reading
    return event;
}

bool ButtonHandler::isButtonPressed(uint8_t buttonNum) {
    if (buttonNum >= 1 && buttonNum <= 6) {
        return buttons[buttonNum - 1].currentState == LOW;
    }
    return false;
}
