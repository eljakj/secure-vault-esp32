#pragma once

#include <Arduino.h>

// =====================================================
// Vault Serializer
// Wandelt Vault-Einträge in Text um und zurück.
// Vorbereitung für spätere Verschlüsselung.
// =====================================================

String vaultSerializeToText();
bool vaultDeserializeFromText(const String& data);

void vaultSerializerDebugPrint();