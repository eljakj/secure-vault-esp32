#pragma once

#include <Arduino.h>

// =====================================================
// TOTP Settings
// Mehrere TOTP Accounts im RAM.
// Wird über /secrets.enc verschlüsselt gespeichert.
// Alte Single-TOTP-Funktionen bleiben kompatibel.
// =====================================================

const int MAX_TOTP_ACCOUNTS = 8;

struct TotpAccount {
  char label[32];
  char base32Secret[128];
};

// Legacy / Kompatibilität
void totpSettingsLoad();
void totpSettingsSave(const char* label, const char* base32Secret);
void totpSettingsClear();

const char* totpSettingsGetLabel();
const char* totpSettingsGetBase32Secret();

bool totpSettingsHasSecret();

void totpSettingsClearRam();
void totpSettingsSetRam(const char* label, const char* base32Secret);
void totpSettingsClearPlaintextFlash();

// Neue Multi-Account API
int totpSettingsGetCount();
bool totpSettingsHasAccounts();

const char* totpSettingsGetLabelAt(int index);
const char* totpSettingsGetBase32SecretAt(int index);

bool totpSettingsAddAccount(const char* label, const char* base32Secret);
bool totpSettingsUpdateAccount(int index, const char* label, const char* base32Secret);
bool totpSettingsDeleteAccount(int index);

void totpSettingsSetCount(int count);
void totpSettingsSetAccountRam(int index, const char* label, const char* base32Secret);