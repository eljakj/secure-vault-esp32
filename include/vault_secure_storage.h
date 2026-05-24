#pragma once

#include <Arduino.h>

// =====================================================
// Encrypted Vault Storage
// Speichert den serialisierten Vault verschlüsselt in LittleFS.
// Datei: /vault.enc
// =====================================================

bool vaultSecureStorageExists();

bool vaultSecureStorageSave();
bool vaultSecureStorageLoad();

bool vaultSecureStorageRemove();

void vaultSecureStorageDebugPrint();