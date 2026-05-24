#include "storage.h"

#include "wifi_settings.h"
#include "totp_settings.h"
#include "vault_secure_storage.h"

void loadPinHashFromFlash(char* storedPinHash) {
  // =====================================================
  // TEMP/DEV MIGRATION:
  // Einmalig gespeicherten PIN auf Default-PIN setzen.
  // Damit wird dein alter PIN 1234 automatisch zu 0000.
  //
  // Danach wird "pin0" gespeichert, damit spätere PIN-Änderungen
  // nicht jedes Mal überschrieben werden.
  // =====================================================

  preferences.begin("migration", false);
  bool pin0000Applied = preferences.getBool("pin0", false);
  preferences.end();

  if (!pin0000Applied) {
    preferences.begin("security", false);
    preferences.putString("pinHash", DEFAULT_PIN_HASH);
    preferences.end();

    // Alter Vault wurde mit altem PIN-Key verschluesselt.
    // Fuer DEV/Test loeschen wir ihn einmalig beim Wechsel auf 0000.
    vaultSecureStorageRemove();

    preferences.begin("migration", false);
    preferences.putBool("pin0", true);
    preferences.end();

    strncpy(storedPinHash, DEFAULT_PIN_HASH, 65);
    Serial.println("PIN reset to default 0000");
    Serial.println("Old encrypted vault removed");
    return;
  }

  preferences.begin("security", false);

  if (!preferences.isKey("pinHash")) {
    preferences.putString("pinHash", DEFAULT_PIN_HASH);
    strncpy(storedPinHash, DEFAULT_PIN_HASH, 65);
    preferences.end();
    return;
  }

  String savedHash = preferences.getString("pinHash", "");

  preferences.end();

  if (savedHash.length() != 64) {
    strncpy(storedPinHash, DEFAULT_PIN_HASH, 65);
    return;
  }

  savedHash.toCharArray(storedPinHash, 65);
}

// =====================================================
// PIN-Hash in Flash speichern
// =====================================================

void savePinHashToFlash(const char* pinHash, char* storedPinHash) {
  preferences.begin("security", false);

  preferences.putString("pinHash", pinHash);

  preferences.end();

  strncpy(storedPinHash, pinHash, 65);
}

// =====================================================
// Recent aus Flash laden
// =====================================================

void loadRecentFromFlash(int* recentIndexes, int maxRecentCount, int& recentCount) {
  preferences.begin("vault", false);

  if (preferences.isKey("recentCount")) {
    recentCount = preferences.getInt("recentCount", 0);
  } else {
    recentCount = 0;
    preferences.putInt("recentCount", 0);
  }

  if (recentCount < 0 || recentCount > maxRecentCount) {
    recentCount = 0;
  }

  for (int i = 0; i < maxRecentCount; i++) {
    String key = "recent" + String(i);

    if (preferences.isKey(key.c_str())) {
      recentIndexes[i] = preferences.getInt(key.c_str(), -1);
    } else {
      recentIndexes[i] = -1;
      preferences.putInt(key.c_str(), -1);
    }
  }

  preferences.end();
}

// =====================================================
// Recent in Flash speichern
// =====================================================

void saveRecentToFlash(const int* recentIndexes, int maxRecentCount, int recentCount) {
  preferences.begin("vault", false);

  preferences.putInt("recentCount", recentCount);

  for (int i = 0; i < maxRecentCount; i++) {
    String key = "recent" + String(i);
    preferences.putInt(key.c_str(), recentIndexes[i]);
  }

  preferences.end();
}

// =====================================================
// Favorites aus Flash laden
// =====================================================

void loadFavoritesFromFlash(int* favoriteIndexes, int maxFavoriteCount, int& favoriteCount) {
  preferences.begin("vault", false);

  if (preferences.isKey("favoriteCount")) {
    favoriteCount = preferences.getInt("favoriteCount", 0);
  } else {
    favoriteCount = 0;
    preferences.putInt("favoriteCount", 0);
  }

  if (favoriteCount < 0 || favoriteCount > maxFavoriteCount) {
    favoriteCount = 0;
  }

  for (int i = 0; i < maxFavoriteCount; i++) {
    String key = "favorite" + String(i);

    if (preferences.isKey(key.c_str())) {
      favoriteIndexes[i] = preferences.getInt(key.c_str(), -1);
    } else {
      favoriteIndexes[i] = -1;
      preferences.putInt(key.c_str(), -1);
    }
  }

  preferences.end();
}

// =====================================================
// Favorites in Flash speichern
// =====================================================

void saveFavoritesToFlash(const int* favoriteIndexes, int maxFavoriteCount, int favoriteCount) {
  preferences.begin("vault", false);

  preferences.putInt("favoriteCount", favoriteCount);

  for (int i = 0; i < maxFavoriteCount; i++) {
    String key = "favorite" + String(i);
    preferences.putInt(key.c_str(), favoriteIndexes[i]);
  }

  preferences.end();
}

// =====================================================
// Alles aus Preferences löschen
// PIN, Favorites, Recent, Settings
// =====================================================

void clearAllStoredData() {
  preferences.begin("security", false);
  preferences.clear();
  preferences.end();

  preferences.begin("vault", false);
  preferences.clear();
  preferences.end();

  preferences.begin("settings", false);
  preferences.clear();
  preferences.end();

  preferences.begin("entries", false);
  preferences.clear();
  preferences.end();

  preferences.begin("portal", false);
  preferences.clear();
  preferences.end();

  preferences.begin("migration", false);
  preferences.clear();
  preferences.end();

  wifiSettingsClear();
  totpSettingsClear();
  vaultSecureStorageRemove();
}