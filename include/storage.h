#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

extern Preferences preferences;

// PIN
void loadPinHashFromFlash(char* storedPinHash);
void savePinHashToFlash(const char* pinHash, char* storedPinHash);

// Recent
void loadRecentFromFlash(int* recentIndexes, int maxRecentCount, int& recentCount);
void saveRecentToFlash(const int* recentIndexes, int maxRecentCount, int recentCount);

// Favorites
void loadFavoritesFromFlash(int* favoriteIndexes, int maxFavoriteCount, int& favoriteCount);
void saveFavoritesToFlash(const int* favoriteIndexes, int maxFavoriteCount, int favoriteCount);

// Factory Reset
void clearAllStoredData();
