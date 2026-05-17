#include "hid_manager.h"
#include "config.h"

#if SECUREVAULT_DEV_MODE == 0
  #include "USB.h"
  #include "USBHIDKeyboard.h"
  static USBHIDKeyboard Keyboard;
#endif

// =====================================================
// USB HID Keyboard
// =====================================================

static KeyboardLayoutMode currentLayout = KEYBOARD_LAYOUT_US;

// =====================================================
// HID initialisieren
// =====================================================

void hidBegin() {
#if SECUREVAULT_DEV_MODE == 0
  Keyboard.begin();
  USB.begin();

  // Kurze Wartezeit, damit der PC das USB-HID-Gerät erkennt.
  delay(800);
#else
  // DEV_MODE:
  // HID bleibt aus, damit Upload/COM-Port einfacher bleibt.
  delay(50);
#endif
}

// =====================================================
// Ist HID aktuell aktiv?
// =====================================================

bool hidIsEnabled() {
#if SECUREVAULT_DEV_MODE == 0
  return true;
#else
  return false;
#endif
}

// =====================================================
// Layout setzen / lesen
// =====================================================

void hidSetKeyboardLayout(KeyboardLayoutMode layout) {
  currentLayout = layout;
}

KeyboardLayoutMode hidGetKeyboardLayout() {
  return currentLayout;
}

void hidToggleKeyboardLayout() {
  if (currentLayout == KEYBOARD_LAYOUT_US) {
    currentLayout = KEYBOARD_LAYOUT_DE;
  } else {
    currentLayout = KEYBOARD_LAYOUT_US;
  }
}

const char* hidGetKeyboardLayoutName() {
  if (currentLayout == KEYBOARD_LAYOUT_DE) {
    return "DE";
  }

  return "US";
}

// =====================================================
// Einzelnes Zeichen im US-HID-Modus senden
// =====================================================

static void sendUSChar(char c) {
#if SECUREVAULT_DEV_MODE == 0
  Keyboard.print(String(c));
#endif
}

// =====================================================
// AltGr+Q für @ auf deutschem Layout
// Hinweis: benötigt HID-Modifier Support.
// =====================================================

static void sendGermanAt() {
#if SECUREVAULT_DEV_MODE == 0
  #ifdef KEY_RIGHT_ALT
    Keyboard.press(KEY_RIGHT_ALT);
    Keyboard.press('q');
    delay(20);
    Keyboard.releaseAll();
  #else
    Keyboard.print("@");
  #endif
#endif
}

// =====================================================
// Ein Zeichen so senden, dass es auf deutschem Windows-Layout stimmt.
// Viele Zeichen werden über die passende US-Taste gemappt.
// =====================================================

static void sendDEChar(char c) {
#if SECUREVAULT_DEV_MODE == 0
  switch (c) {
    // QWERTZ Tausch
    case 'y': Keyboard.print("z"); return;
    case 'Y': Keyboard.print("Z"); return;
    case 'z': Keyboard.print("y"); return;
    case 'Z': Keyboard.print("Y"); return;

    // Sonderzeichen, die auf deutscher Tastatur andere Tasten brauchen
    case '@': sendGermanAt(); return;
    case '#': Keyboard.print("\\"); return;
    case '&': Keyboard.print("^"); return;
    case '*': Keyboard.print("}"); return;
    case '_': Keyboard.print("?"); return;
    case '-': Keyboard.print("/"); return;
    case '+': Keyboard.print("]"); return;
    case '=': Keyboard.print(")"); return;
    case '?': Keyboard.print("_"); return;
    case '/': Keyboard.print("&"); return;

    // ^ ist auf DE ein Dead-Key. Grave-Taste + Space ergibt meistens ^
    case '^':
      Keyboard.print("`");
      delay(10);
      Keyboard.print(" ");
      return;

    default:
      Keyboard.print(String(c));
      return;
  }
#endif
}

// =====================================================
// Text senden je nach Layout
// =====================================================

void hidSendText(const char* text) {
  if (text == nullptr) return;

#if SECUREVAULT_DEV_MODE == 0
  if (currentLayout == KEYBOARD_LAYOUT_US) {
    Keyboard.print(text);
    return;
  }

  for (int i = 0; text[i] != '\0'; i++) {
    sendDEChar(text[i]);
    delay(2);
  }
#else
  // DEV_MODE: nichts tippen.
  (void)text;
#endif
}

// =====================================================
// Username senden
// =====================================================

void hidSendUsername(const char* username) {
  hidSendText(username);
}

// =====================================================
// Passwort senden
// =====================================================

void hidSendPassword(const char* password) {
  hidSendText(password);
}

// =====================================================
// Username + TAB + Passwort senden
// =====================================================

void hidSendUsernameTabPassword(const char* username, const char* password) {
  if (username == nullptr || password == nullptr) return;

#if SECUREVAULT_DEV_MODE == 0
  hidSendText(username);
  Keyboard.write(KEY_TAB);
  hidSendText(password);
#else
  (void)username;
  (void)password;
#endif
}
