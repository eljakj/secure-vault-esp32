#pragma once

#include <Arduino.h>

// =====================================================
// Encrypted Secrets Storage
// Speichert Wi-Fi + TOTP Settings verschlüsselt in LittleFS.
// Datei: /secrets.enc
// =====================================================

bool secretsSecureStorageExists();

bool secretsSecureStorageSave();
bool secretsSecureStorageLoad();

bool secretsSecureStorageRemove();

void secretsSecureStorageDebugPrint();