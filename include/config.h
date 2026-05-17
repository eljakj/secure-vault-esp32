#pragma once

#include <TFT_eSPI.h>


// =====================================================
// DEV / PRODUCT MODE
// =====================================================
//
// 1 = Entwicklungsmodus:
//     - USB HID Keyboard ist deaktiviert
//     - COM-Port / Upload ist meistens einfacher
//     - Auto-Type zeigt nur "disabled"
//
// 0 = Produktmodus:
//     - USB HID Keyboard ist aktiv
//     - Auto-Type funktioniert
//     - COM-Port kann nach Reset verschwinden, weil Gerät als Keyboard startet
//
// Zum Testen von Upload/COM: 1
// Zum Testen von Auto-Type: 0
//
#define SECUREVAULT_DEV_MODE 1

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

// SHA-256 Hash von Demo-PIN "1234"
// Wird nur benutzt, wenn im Flash noch kein PIN-Hash existiert.
const char DEFAULT_PIN_HASH[] =
  "03ac674216f3e15c761ee1a5e255f067"
  "953623c8b388b4459e13f978d7c846f4";


// =====================================================
// Wi-Fi für TOTP-Zeit-Sync
// WICHTIG: Für echte Produkte später nicht fest im Code speichern.
// =====================================================

const char WIFI_SSID[] = "YOUR_WIFI_NAME";
const char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";
