#include "wifi_settings.h"

#include <Preferences.h>

extern Preferences preferences;

// =====================================================
// Wi-Fi Daten im RAM
// =====================================================

static char savedWifiSsid[64] = "";
static char savedWifiPassword[96] = "";

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
// Wi-Fi Daten aus Flash laden
// =====================================================

void wifiSettingsLoad() {
  preferences.begin("wifi", true);

  String ssid = preferences.getString("ssid", "");
  String password = preferences.getString("pass", "");

  preferences.end();

  safeCopy(savedWifiSsid, sizeof(savedWifiSsid), ssid.c_str());
  safeCopy(savedWifiPassword, sizeof(savedWifiPassword), password.c_str());
}

// =====================================================
// Wi-Fi Daten speichern
// =====================================================

void wifiSettingsSave(const char* ssid, const char* password) {
  safeCopy(savedWifiSsid, sizeof(savedWifiSsid), ssid);
  safeCopy(savedWifiPassword, sizeof(savedWifiPassword), password);

  preferences.begin("wifi", false);
  preferences.putString("ssid", savedWifiSsid);
  preferences.putString("pass", savedWifiPassword);
  preferences.end();
}

// =====================================================
// Wi-Fi Daten löschen
// =====================================================

void wifiSettingsClear() {
  savedWifiSsid[0] = '\0';
  savedWifiPassword[0] = '\0';

  preferences.begin("wifi", false);
  preferences.clear();
  preferences.end();
}

// =====================================================
// Getter
// =====================================================

const char* wifiSettingsGetSsid() {
  return savedWifiSsid;
}

const char* wifiSettingsGetPassword() {
  return savedWifiPassword;
}

bool wifiSettingsHasCredentials() {
  return strlen(savedWifiSsid) > 0 && strlen(savedWifiPassword) > 0;
}

// =====================================================
// Wi-Fi Daten nur aus RAM löschen
// Flash bleibt erhalten.
// Wichtig beim Lock.
// =====================================================

void wifiSettingsClearRam() {
  memset(savedWifiSsid, 0, sizeof(savedWifiSsid));
  memset(savedWifiPassword, 0, sizeof(savedWifiPassword));
}

// =====================================================
// Wi-Fi Daten nur in RAM setzen
// Flash bleibt unverändert.
// Wird später für encrypted secrets storage benutzt.
// =====================================================

void wifiSettingsSetRam(const char* ssid, const char* password) {
  safeCopy(savedWifiSsid, sizeof(savedWifiSsid), ssid);
  safeCopy(savedWifiPassword, sizeof(savedWifiPassword), password);
}