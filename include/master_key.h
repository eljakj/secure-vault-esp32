#pragma once

#include <Arduino.h>

// =====================================================
// Master-Key im RAM
// Vorbereitung für spätere Vault-Verschlüsselung
// =====================================================

void masterKeyDeriveFromPin(const char* pin);
void masterKeyReplaceWithPin(const char* pin);
void masterKeyClear();

bool masterKeyIsReady();

const uint8_t* masterKeyGetBytes();
size_t masterKeyGetLength();