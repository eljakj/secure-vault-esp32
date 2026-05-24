#pragma once

#include <Arduino.h>

// =====================================================
// Wi-Fi Settings für TOTP-Zeit-Sync
// =====================================================

void wifiSettingsLoad();
void wifiSettingsSave(const char* ssid, const char* password);
void wifiSettingsClear();

const char* wifiSettingsGetSsid();
const char* wifiSettingsGetPassword();

bool wifiSettingsHasCredentials();

void wifiSettingsClearRam();
void wifiSettingsSetRam(const char* ssid, const char* password);