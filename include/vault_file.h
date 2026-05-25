#pragma once

#include <Arduino.h>

// =====================================================
// Vault File Storage
// Hilfsfunktionen für LittleFS Textdateien.
// Wird für /vault.enc, /secrets.enc und Backup verwendet.
// =====================================================

bool vaultFileBegin();

bool vaultFileWriteText(const char* path, const String& content);
bool vaultFileReadText(const char* path, String& output);
bool vaultFileExists(const char* path);
bool vaultFileRemove(const char* path);

void vaultFileDebugPrintInfo();