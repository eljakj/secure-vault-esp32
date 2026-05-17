#include "buttons.h"

static bool lastDownButtonState = HIGH;
static bool lastSelectButtonState = HIGH;

static unsigned long lastDownPressTime = 0;
static unsigned long selectPressStartTime = 0;

static bool selectIsPressed = false;

// =====================================================
// Buttons initialisieren
// =====================================================

void buttonsBegin() {
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT, INPUT_PULLUP);
}

// =====================================================
// DOWN-Button Event erkennen
// =====================================================

static ButtonEvent readDownButtonEvent() {
  bool currentState = digitalRead(BUTTON_DOWN);
  unsigned long now = millis();

  ButtonEvent event = BUTTON_NONE;

  if (lastDownButtonState == HIGH && currentState == LOW) {
    if (now - lastDownPressTime > BUTTON_DEBOUNCE_MS) {
      lastDownPressTime = now;
      event = BUTTON_DOWN_SHORT;
    }
  }

  lastDownButtonState = currentState;
  return event;
}

// =====================================================
// SELECT-Button Event erkennen
// Kurz drücken = öffnen / anzeigen
// Lang drücken = zurück / löschen
// =====================================================

static ButtonEvent readSelectButtonEvent() {
  bool currentState = digitalRead(BUTTON_SELECT);
  unsigned long now = millis();

  ButtonEvent event = BUTTON_NONE;

  if (lastSelectButtonState == HIGH && currentState == LOW) {
    selectPressStartTime = now;
    selectIsPressed = true;
  }

  if (lastSelectButtonState == LOW && currentState == HIGH && selectIsPressed) {
    unsigned long pressDuration = now - selectPressStartTime;

    if (pressDuration >= LONG_PRESS_MS) {
      event = BUTTON_SELECT_LONG;
    } else if (pressDuration >= BUTTON_DEBOUNCE_MS) {
      event = BUTTON_SELECT_SHORT;
    }

    selectIsPressed = false;
  }

  lastSelectButtonState = currentState;
  return event;
}

// =====================================================
// Ein kombiniertes Button-Event zurückgeben
// SELECT hat Priorität vor DOWN.
// =====================================================

ButtonEvent readButtonEvent() {
  ButtonEvent downEvent = readDownButtonEvent();
  ButtonEvent selectEvent = readSelectButtonEvent();

  if (selectEvent != BUTTON_NONE) {
    return selectEvent;
  }

  if (downEvent != BUTTON_NONE) {
    return downEvent;
  }

  return BUTTON_NONE;
}
