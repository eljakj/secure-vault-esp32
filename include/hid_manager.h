#pragma once

#include <Arduino.h>

enum KeyboardLayoutMode {
  KEYBOARD_LAYOUT_US = 0,
  KEYBOARD_LAYOUT_DE = 1
};

void hidBegin();
bool hidIsEnabled();

void hidSetKeyboardLayout(KeyboardLayoutMode layout);
KeyboardLayoutMode hidGetKeyboardLayout();
void hidToggleKeyboardLayout();
const char* hidGetKeyboardLayoutName();

void hidSendText(const char* text);
void hidSendUsername(const char* username);
void hidSendPassword(const char* password);
void hidSendUsernameTabPassword(const char* username, const char* password);
