#pragma once

#include <Arduino.h>

// =====================================================
// TOTP Settings
// Speichert Label + Base32 Secret im Flash
// =====================================================

void totpSettingsLoad();
void totpSettingsSave(const char* label, const char* base32Secret);
void totpSettingsClear();

const char* totpSettingsGetLabel();
const char* totpSettingsGetBase32Secret();

bool totpSettingsHasSecret();

void totpSettingsClearRam();
void totpSettingsSetRam(const char* label, const char* base32Secret);