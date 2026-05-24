#include "vault_file.h"

#include <LittleFS.h>

// =====================================================
// LittleFS starten
// =====================================================

bool vaultFileBegin() {
  if (LittleFS.begin(true)) {
    return true;
  }

  return false;
}

// =====================================================
// Text-Datei schreiben
// =====================================================

bool vaultFileWriteText(const char* path, const String& content) {
  if (path == nullptr || strlen(path) == 0) {
    return false;
  }

  File file = LittleFS.open(path, "w");

  if (!file) {
    return false;
  }

  size_t written = file.print(content);
  file.close();

  return written == content.length();
}

// =====================================================
// Text-Datei lesen
// =====================================================

bool vaultFileReadText(const char* path, String& output) {
  output = "";

  if (path == nullptr || strlen(path) == 0) {
    return false;
  }

  if (!LittleFS.exists(path)) {
    return false;
  }

  File file = LittleFS.open(path, "r");

  if (!file) {
    return false;
  }

  while (file.available()) {
    output += (char)file.read();
  }

  file.close();
  return true;
}

// =====================================================
// Existiert Datei?
// =====================================================

bool vaultFileExists(const char* path) {
  if (path == nullptr || strlen(path) == 0) {
    return false;
  }

  return LittleFS.exists(path);
}

// =====================================================
// Datei löschen
// =====================================================

bool vaultFileRemove(const char* path) {
  if (path == nullptr || strlen(path) == 0) {
    return false;
  }

  if (!LittleFS.exists(path)) {
    return true;
  }

  return LittleFS.remove(path);
}

// =====================================================
// Debug Info
// =====================================================

void vaultFileDebugPrintInfo() {
  Serial.println();
  Serial.println("=== LittleFS Info ===");
  Serial.print("Total bytes: ");
  Serial.println(LittleFS.totalBytes());
  Serial.print("Used bytes: ");
  Serial.println(LittleFS.usedBytes());
  Serial.println("=====================");
}