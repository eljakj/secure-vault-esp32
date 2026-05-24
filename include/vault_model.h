#pragma once

#include <Arduino.h>

// =====================================================
// Professionelle Vault-Datenstruktur
// =====================================================

const int MAX_VAULT_ENTRIES = 20;

struct VaultEntry {
  char title[32];
  char username[64];
  char password[96];
  char category[24];
};

extern VaultEntry vaultEntries[MAX_VAULT_ENTRIES];
extern int vaultCount;

bool vaultIsValidIndex(int index);

const char* vaultGetTitle(int index);
const char* vaultGetUsername(int index);
const char* vaultGetPassword(int index);
const char* vaultGetCategory(int index);


// =====================================================
// Vault speichern/laden
// =====================================================

void vaultRestoreDefaultEntries();
void vaultLoadFromFlash();
void vaultSaveToFlash();
void vaultClearPlaintextFlash();
void vaultClearRam();

bool vaultAddEntry(const char* title, const char* username, const char* password, const char* category);
bool vaultDeleteEntry(int index);

bool vaultUpdateEntry(int index, const char* title, const char* username, const char* password, const char* category);