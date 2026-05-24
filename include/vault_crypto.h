#pragma once

#include <Arduino.h>

// =====================================================
// Vault Crypto
// AES-256-CBC Vorbereitung für verschlüsselte Vault-Datei
// =====================================================

bool vaultCryptoEncryptText(const String& plainText, String& encryptedOutput);
bool vaultCryptoDecryptText(const String& encryptedInput, String& plainTextOutput);

void vaultCryptoDebugTest();