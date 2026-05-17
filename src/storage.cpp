#include "storage.h"
#include "wifi_settings.h"

// =====================================================
// PIN-Hash aus Flash laden
// =====================================================

void loadPinHashFromFlash(char* storedPinHash) {
  preferences.begin("security", true);

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
  preferences.begin("vault", true);

  recentCount = preferences.getInt("recentCount", 0);

  if (recentCount < 0 || recentCount > maxRecentCount) {
    recentCount = 0;
  }

  for (int i = 0; i < maxRecentCount; i++) {
    String key = "recent" + String(i);
    recentIndexes[i] = preferences.getInt(key.c_str(), -1);
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
  preferences.begin("vault", true);

  favoriteCount = preferences.getInt("favoriteCount", 0);

  if (favoriteCount < 0 || favoriteCount > maxFavoriteCount) {
    favoriteCount = 0;
  }

  for (int i = 0; i < maxFavoriteCount; i++) {
    String key = "favorite" + String(i);
    favoriteIndexes[i] = preferences.getInt(key.c_str(), -1);
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

  wifiSettingsClear();
}
