#pragma once

#include <TFT_eSPI.h>

// =====================================================
// DEV / PRODUCT MODE
// =====================================================
//
// SECUREVAULT_DEV_MODE kommt normalerweise aus platformio.ini:
//
// -DSECUREVAULT_DEV_MODE=1
//   Entwicklungsmodus:
//   - USB HID Keyboard ist deaktiviert
//   - COM-Port / Upload ist einfacher
//
// -DSECUREVAULT_DEV_MODE=0
//   Produkt/Test-Modus:
//   - USB HID Keyboard ist aktiv
//   - Auto-Type funktioniert
//

#ifndef SECUREVAULT_DEV_MODE
#define SECUREVAULT_DEV_MODE 1
#endif

// =====================================================
// Hardware: LILYGO T-Display-S3
// =====================================================

#define PIN_POWER_ON 15
#define PIN_LCD_BL 38

#define BUTTON_DOWN   0
#define BUTTON_SELECT 14

// =====================================================
// Display
// =====================================================

const int SCREEN_WIDTH = 170;
const int SCREEN_HEIGHT = 320;

// =====================================================
// Farben
// =====================================================

#define COLOR_BG        TFT_BLACK
#define COLOR_TEXT      TFT_WHITE
#define COLOR_MUTED     TFT_DARKGREY
#define COLOR_ACCENT    TFT_CYAN
#define COLOR_SELECTED  TFT_BLUE
#define COLOR_LINE      TFT_DARKGREY

// =====================================================
// Timing
// =====================================================

const unsigned long BUTTON_DEBOUNCE_MS = 120;
const unsigned long LONG_PRESS_MS = 700;
const unsigned long AUTO_LOCK_MS = 60000;

// =====================================================
// PIN Security
// =====================================================

const int MAX_PIN_ATTEMPTS = 3;
const unsigned long PIN_LOCKOUT_MS = 30000;

// SHA-256 Hash von Demo-PIN "0000"
// Wird nur benutzt, wenn im Flash noch kein PIN-Hash existiert.
const char DEFAULT_PIN_HASH[] =
  "9af15b336e6a9619928537df30b2e6a2"
  "376569fcf9d7e773eccede65606529a0";