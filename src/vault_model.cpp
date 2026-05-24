#include "vault_model.h"

#include <Preferences.h>

#include "master_key.h"
#include "vault_secure_storage.h"

extern Preferences preferences;

// =====================================================
// Standard-Demo-Daten
// Später ersetzen wir das durch verschlüsselte Daten.
// =====================================================

// Ab jetzt startet das Gerät ohne Demo-Passwörter.
// Einträge werden über das lokale Web Portal hinzugefügt.
static const int DEFAULT_VAULT_COUNT = 0;

VaultEntry vaultEntries[MAX_VAULT_ENTRIES];
int vaultCount = 0;

// =====================================================
// Eintrag im RAM leeren
// =====================================================

static void clearVaultEntry(int index) {
  if (index < 0 || index >= MAX_VAULT_ENTRIES) {
    return;
  }

  vaultEntries[index].title[0] = '\0';
  vaultEntries[index].username[0] = '\0';
  vaultEntries[index].password[0] = '\0';
  vaultEntries[index].category[0] = '\0';
}

// =====================================================
// Sicheres Kopieren in char[]
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
// Vault-Änderungen speichern
// Ab jetzt NICHT mehr als Klartext in Preferences speichern.
// Hauptspeicher ist /vault.enc über LittleFS.
// =====================================================

static bool vaultPersistChanges() {
  if (!masterKeyIsReady()) {
    Serial.println("Encrypted vault save: FAILED, master key not ready");
    return false;
  }

  bool secureOk = vaultSecureStorageSave();

  Serial.print("Encrypted vault save: ");
  Serial.println(secureOk ? "OK" : "FAILED");

  return secureOk;
}

// =====================================================
// Standard-Einträge wiederherstellen
// =====================================================

void vaultRestoreDefaultEntries() {
  vaultCount = DEFAULT_VAULT_COUNT;

  for (int i = 0; i < MAX_VAULT_ENTRIES; i++) {
    clearVaultEntry(i);
  }
}

// =====================================================
// Vault aus Flash laden
// =====================================================

void vaultLoadFromFlash() {
  preferences.begin("entries", true);

  bool initialized = preferences.getBool("initialized", false);
  int savedCount = preferences.getInt("count", 0);

  if (!initialized || savedCount < 0 || savedCount > MAX_VAULT_ENTRIES) {
    preferences.end();
    vaultRestoreDefaultEntries();
    return;
  }

  vaultCount = savedCount;

  for (int i = 0; i < vaultCount; i++) {
    String titleKey = "t" + String(i);
    String userKey = "u" + String(i);
    String passKey = "p" + String(i);
    String catKey = "c" + String(i);

    String title = preferences.getString(titleKey.c_str(), "");
    String username = preferences.getString(userKey.c_str(), "");
    String password = preferences.getString(passKey.c_str(), "");
    String category = preferences.getString(catKey.c_str(), "");

    safeCopy(vaultEntries[i].title, sizeof(vaultEntries[i].title), title.c_str());
    safeCopy(vaultEntries[i].username, sizeof(vaultEntries[i].username), username.c_str());
    safeCopy(vaultEntries[i].password, sizeof(vaultEntries[i].password), password.c_str());
    safeCopy(vaultEntries[i].category, sizeof(vaultEntries[i].category), category.c_str());
  }

  for (int i = vaultCount; i < MAX_VAULT_ENTRIES; i++) {
    clearVaultEntry(i);
  }

  preferences.end();
}

// =====================================================
// Vault in Flash speichern
// =====================================================

void vaultSaveToFlash() {
  preferences.begin("entries", false);
  preferences.clear();

  preferences.putBool("initialized", true);
  preferences.putInt("count", vaultCount);

  for (int i = 0; i < vaultCount; i++) {
    String titleKey = "t" + String(i);
    String userKey = "u" + String(i);
    String passKey = "p" + String(i);
    String catKey = "c" + String(i);

    preferences.putString(titleKey.c_str(), vaultEntries[i].title);
    preferences.putString(userKey.c_str(), vaultEntries[i].username);
    preferences.putString(passKey.c_str(), vaultEntries[i].password);
    preferences.putString(catKey.c_str(), vaultEntries[i].category);
  }

  preferences.end();
}

// =====================================================
// Alte Klartext-Vault-Daten aus Preferences löschen
// Wichtig nach Umstieg auf /vault.enc
// =====================================================

void vaultClearPlaintextFlash() {
  // Diese Migration soll nur einmal laufen.
  // Danach benutzen wir /vault.enc als Hauptspeicher.
  preferences.begin("migration", false);
  bool alreadyCleared = preferences.getBool("plainVaultCleared", false);

  if (alreadyCleared) {
    preferences.end();
    return;
  }

  preferences.end();

  preferences.begin("entries", false);
  preferences.clear();
  preferences.end();

  preferences.begin("migration", false);
  preferences.putBool("plainVaultCleared", true);
  preferences.end();

  Serial.println("Plaintext vault Preferences cleared");
}

// =====================================================
// Vault komplett aus RAM löschen
// Wichtig beim Lock:
// Passwörter sollen nicht im RAM bleiben, wenn Gerät gesperrt ist.
// =====================================================

void vaultClearRam() {
  for (int i = 0; i < MAX_VAULT_ENTRIES; i++) {
    memset(vaultEntries[i].title, 0, sizeof(vaultEntries[i].title));
    memset(vaultEntries[i].username, 0, sizeof(vaultEntries[i].username));
    memset(vaultEntries[i].password, 0, sizeof(vaultEntries[i].password));
    memset(vaultEntries[i].category, 0, sizeof(vaultEntries[i].category));
  }

  vaultCount = 0;

  Serial.println("Vault RAM cleared");
}

// =====================================================
// Eintrag hinzufügen
// =====================================================

bool vaultAddEntry(const char* title, const char* username, const char* password,
                   const char* category) {
  if (vaultCount >= MAX_VAULT_ENTRIES) {
    return false;
  }

  if (title == nullptr || strlen(title) == 0) {
    return false;
  }

  safeCopy(vaultEntries[vaultCount].title, sizeof(vaultEntries[vaultCount].title), title);
  safeCopy(vaultEntries[vaultCount].username, sizeof(vaultEntries[vaultCount].username), username);
  safeCopy(vaultEntries[vaultCount].password, sizeof(vaultEntries[vaultCount].password), password);
  safeCopy(vaultEntries[vaultCount].category, sizeof(vaultEntries[vaultCount].category), category);

  vaultCount++;

  if (!vaultPersistChanges()) {
    return false;
  }

  return true;
}

// =====================================================
// Eintrag bearbeiten
// =====================================================

bool vaultUpdateEntry(int index, const char* title, const char* username, const char* password,
                      const char* category) {
  if (!vaultIsValidIndex(index)) {
    return false;
  }

  if (title == nullptr || strlen(title) == 0) {
    return false;
  }

  safeCopy(vaultEntries[index].title, sizeof(vaultEntries[index].title), title);
  safeCopy(vaultEntries[index].username, sizeof(vaultEntries[index].username), username);

  // Wenn password leer ist, behalten wir das alte Passwort.
  if (password != nullptr && strlen(password) > 0) {
    safeCopy(vaultEntries[index].password, sizeof(vaultEntries[index].password), password);
  }

  safeCopy(vaultEntries[index].category, sizeof(vaultEntries[index].category), category);

  if (!vaultPersistChanges()) {
    return false;
  }
  return true;
}

// =====================================================
// Eintrag löschen
// =====================================================

bool vaultDeleteEntry(int index) {
  if (!vaultIsValidIndex(index)) {
    return false;
  }

  for (int i = index; i < vaultCount - 1; i++) {
    vaultEntries[i] = vaultEntries[i + 1];
  }

  vaultCount--;

  if (vaultCount < 0) {
    vaultCount = 0;
  }

  if (vaultCount < MAX_VAULT_ENTRIES) {
    clearVaultEntry(vaultCount);
  }

  if (!vaultPersistChanges()) {
    return false;
  }

  return true;
}

// =====================================================
// Getter
// =====================================================

bool vaultIsValidIndex(int index) {
  return index >= 0 && index < vaultCount && index < MAX_VAULT_ENTRIES;
}

const char* vaultGetTitle(int index) {
  if (!vaultIsValidIndex(index))
    return "";
  return vaultEntries[index].title;
}

const char* vaultGetUsername(int index) {
  if (!vaultIsValidIndex(index))
    return "";
  return vaultEntries[index].username;
}

const char* vaultGetPassword(int index) {
  if (!vaultIsValidIndex(index))
    return "";
  return vaultEntries[index].password;
}

const char* vaultGetCategory(int index) {
  if (!vaultIsValidIndex(index))
    return "";
  return vaultEntries[index].category;
}