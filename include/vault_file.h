#pragma once

#include <Arduino.h>

// =====================================================
// Vault File Storage
// Vorbereitung für spätere verschlüsselte Datei
// =====================================================

bool vaultFileBegin();

bool vaultFileWriteText(const char* path, const String& content);
bool vaultFileReadText(const char* path, String& output);
bool vaultFileExists(const char* path);
bool vaultFileRemove(const char* path);

void vaultFileDebugPrintInfo();