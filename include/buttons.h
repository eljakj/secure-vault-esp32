#pragma once

#include <Arduino.h>
#include "config.h"

// =====================================================
// Button-Events
// =====================================================

enum ButtonEvent {
  BUTTON_NONE,
  BUTTON_DOWN_SHORT,
  BUTTON_SELECT_SHORT,
  BUTTON_SELECT_LONG
};

void buttonsBegin();
ButtonEvent readButtonEvent();
