#include "totp_settings.h"

#include <Preferences.h>

extern Preferences preferences;

// =====================================================
// TOTP Daten im RAM
// =====================================================

static TotpAccount totpAccounts[MAX_TOTP_ACCOUNTS];
static int totpAccountCount = 0;

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

static void clearAccount(int index) {
  if (index < 0 || index >= MAX_TOTP_ACCOUNTS) {
    return;
  }

  memset(totpAccounts[index].label, 0, sizeof(totpAccounts[index].label));
  memset(totpAccounts[index].base32Secret, 0, sizeof(totpAccounts[index].base32Secret));
}

// =====================================================
// Legacy: alte Klartext-Preferences laden
// Wird nur noch für Migration benutzt.
// =====================================================

void totpSettingsLoad() {
  preferences.begin("totp", true);

  String label = preferences.getString("label", "");
  String secret = preferences.getString("secret", "");

  preferences.end();

  totpSettingsClearRam();

  if (secret.length() > 0) {
    totpAccountCount = 1;
    safeCopy(totpAccounts[0].label, sizeof(totpAccounts[0].label), label.c_str());
    safeCopy(totpAccounts[0].base32Secret, sizeof(totpAccounts[0].base32Secret), secret.c_str());
  }
}

// =====================================================
// Legacy: speichert noch in Preferences
// Möglichst nicht mehr neu benutzen.
// Web Portal benutzt jetzt SetRam + /secrets.enc.
// =====================================================

void totpSettingsSave(const char* label, const char* base32Secret) {
  totpSettingsSetRam(label, base32Secret);

  preferences.begin("totp", false);
  preferences.putString("label", totpSettingsGetLabel());
  preferences.putString("secret", totpSettingsGetBase32Secret());
  preferences.end();
}

// =====================================================
// Legacy Clear
// =====================================================

void totpSettingsClear() {
  totpSettingsClearRam();

  preferences.begin("totp", false);
  preferences.clear();
  preferences.end();
}

// =====================================================
// Legacy Getter: Account 0
// =====================================================

const char* totpSettingsGetLabel() {
  return totpSettingsGetLabelAt(0);
}

const char* totpSettingsGetBase32Secret() {
  return totpSettingsGetBase32SecretAt(0);
}

bool totpSettingsHasSecret() {
  return totpAccountCount > 0 && strlen(totpAccounts[0].base32Secret) > 0;
}

// =====================================================
// RAM löschen
// =====================================================

void totpSettingsClearRam() {
  for (int i = 0; i < MAX_TOTP_ACCOUNTS; i++) {
    clearAccount(i);
  }

  totpAccountCount = 0;
}

// =====================================================
// Legacy RAM Setter: setzt Account 0 und löscht Rest
// =====================================================

void totpSettingsSetRam(const char* label, const char* base32Secret) {
  totpSettingsClearRam();

  if (base32Secret == nullptr || strlen(base32Secret) == 0) {
    return;
  }

  totpAccountCount = 1;

  safeCopy(totpAccounts[0].label, sizeof(totpAccounts[0].label), label);
  safeCopy(totpAccounts[0].base32Secret, sizeof(totpAccounts[0].base32Secret), base32Secret);
}

// =====================================================
// Alte TOTP Klartextdaten aus Preferences löschen
// =====================================================

void totpSettingsClearPlaintextFlash() {
  preferences.begin("totp", false);
  preferences.clear();
  preferences.end();
}

// =====================================================
// Neue Multi-Account API
// =====================================================

int totpSettingsGetCount() {
  return totpAccountCount;
}

bool totpSettingsHasAccounts() {
  return totpAccountCount > 0;
}

const char* totpSettingsGetLabelAt(int index) {
  if (index < 0 || index >= totpAccountCount) {
    return "TOTP";
  }

  if (strlen(totpAccounts[index].label) == 0) {
    return "TOTP";
  }

  return totpAccounts[index].label;
}

const char* totpSettingsGetBase32SecretAt(int index) {
  if (index < 0 || index >= totpAccountCount) {
    return "";
  }

  return totpAccounts[index].base32Secret;
}

bool totpSettingsAddAccount(const char* label, const char* base32Secret) {
  if (totpAccountCount >= MAX_TOTP_ACCOUNTS) {
    return false;
  }

  if (base32Secret == nullptr || strlen(base32Secret) == 0) {
    return false;
  }

  int index = totpAccountCount;

  safeCopy(totpAccounts[index].label, sizeof(totpAccounts[index].label), label);
  safeCopy(totpAccounts[index].base32Secret, sizeof(totpAccounts[index].base32Secret),
           base32Secret);

  totpAccountCount++;
  return true;
}

bool totpSettingsUpdateAccount(int index, const char* label, const char* base32Secret) {
  if (index < 0 || index >= totpAccountCount) {
    return false;
  }

  safeCopy(totpAccounts[index].label, sizeof(totpAccounts[index].label), label);

  if (base32Secret != nullptr && strlen(base32Secret) > 0) {
    safeCopy(totpAccounts[index].base32Secret, sizeof(totpAccounts[index].base32Secret),
             base32Secret);
  }

  return true;
}

bool totpSettingsDeleteAccount(int index) {
  if (index < 0 || index >= totpAccountCount) {
    return false;
  }

  for (int i = index; i < totpAccountCount - 1; i++) {
    totpAccounts[i] = totpAccounts[i + 1];
  }

  totpAccountCount--;

  if (totpAccountCount < 0) {
    totpAccountCount = 0;
  }

  clearAccount(totpAccountCount);

  return true;
}

void totpSettingsSetCount(int count) {
  if (count < 0) {
    count = 0;
  }

  if (count > MAX_TOTP_ACCOUNTS) {
    count = MAX_TOTP_ACCOUNTS;
  }

  totpAccountCount = count;

  for (int i = totpAccountCount; i < MAX_TOTP_ACCOUNTS; i++) {
    clearAccount(i);
  }
}

void totpSettingsSetAccountRam(int index, const char* label, const char* base32Secret) {
  if (index < 0 || index >= MAX_TOTP_ACCOUNTS) {
    return;
  }

  safeCopy(totpAccounts[index].label, sizeof(totpAccounts[index].label), label);
  safeCopy(totpAccounts[index].base32Secret, sizeof(totpAccounts[index].base32Secret),
           base32Secret);

  if (index >= totpAccountCount) {
    totpAccountCount = index + 1;
  }
}