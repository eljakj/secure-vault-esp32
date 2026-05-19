#include "totp_settings.h"

#include <Preferences.h>

extern Preferences preferences;

// =====================================================
// TOTP Daten im RAM
// =====================================================

static char savedTotpLabel[32] = "";
static char savedTotpBase32Secret[128] = "";

// =====================================================
// Hilfsfunktion: sicher kopieren
// =====================================================

static void safeCopy(char* destination, size_t destinationSize, const char* source) {
  if (destination == nullptr || destinationSize == 0) {
    return;
  }

  if (source == nullptr) {
    destination[0] = '\0';
    return;
  }

  strncpy(destination, source, destinationSize - 1);
  destination[destinationSize - 1] = '\0';
}

// =====================================================
// TOTP Settings laden
// =====================================================

void totpSettingsLoad() {
  preferences.begin("totp", true);

  String label = preferences.getString("label", "");
  String secret = preferences.getString("secret", "");

  preferences.end();

  safeCopy(savedTotpLabel, sizeof(savedTotpLabel), label.c_str());
  safeCopy(savedTotpBase32Secret, sizeof(savedTotpBase32Secret), secret.c_str());
}

// =====================================================
// TOTP Settings speichern
// =====================================================

void totpSettingsSave(const char* label, const char* base32Secret) {
  safeCopy(savedTotpLabel, sizeof(savedTotpLabel), label);
  safeCopy(savedTotpBase32Secret, sizeof(savedTotpBase32Secret), base32Secret);

  preferences.begin("totp", false);
  preferences.putString("label", savedTotpLabel);
  preferences.putString("secret", savedTotpBase32Secret);
  preferences.end();
}

// =====================================================
// TOTP Settings löschen
// =====================================================

void totpSettingsClear() {
  savedTotpLabel[0] = '\0';
  savedTotpBase32Secret[0] = '\0';

  preferences.begin("totp", false);
  preferences.clear();
  preferences.end();
}

// =====================================================
// Getter
// =====================================================

const char* totpSettingsGetLabel() {
  if (strlen(savedTotpLabel) == 0) {
    return "TOTP";
  }

  return savedTotpLabel;
}

const char* totpSettingsGetBase32Secret() {
  return savedTotpBase32Secret;
}

bool totpSettingsHasSecret() {
  return strlen(savedTotpBase32Secret) > 0;
}