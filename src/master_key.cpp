#include "master_key.h"

#include "mbedtls/sha256.h"

// =====================================================
// Master-Key Zustand
// =====================================================

static uint8_t masterKey[32];
static bool masterKeyReady = false;

// Fester Projekt-Salt für den ersten Schritt.
// Später ersetzen wir das durch einen zufälligen Salt,
// der im Gerät gespeichert wird.
static const char* MASTER_KEY_SALT = "SecureVault-v1-local-device-salt";

// =====================================================
// RAM sicher leeren
// =====================================================

static void secureClear(void* data, size_t length) {
  if (data == nullptr || length == 0) {
    return;
  }

  volatile uint8_t* p = (volatile uint8_t*)data;

  while (length--) {
    *p++ = 0;
  }
}

// =====================================================
// Master-Key aus PIN ableiten
// =====================================================

void masterKeyDeriveFromPin(const char* pin) {
  masterKeyClear();

  if (pin == nullptr || strlen(pin) == 0) {
    return;
  }

  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);

  // Für Step 48A:
  // Key = SHA256("SecureVault..." + PIN)
  // Später ersetzen wir das durch PBKDF2 oder HKDF.
  mbedtls_sha256_update(&ctx, (const unsigned char*)MASTER_KEY_SALT, strlen(MASTER_KEY_SALT));

  mbedtls_sha256_update(&ctx, (const unsigned char*)pin, strlen(pin));

  mbedtls_sha256_finish(&ctx, masterKey);
  mbedtls_sha256_free(&ctx);

  masterKeyReady = true;
}

// =====================================================
// Master-Key aus RAM löschen
// =====================================================

void masterKeyClear() {
  secureClear(masterKey, sizeof(masterKey));
  masterKeyReady = false;
}

// =====================================================
// Getter
// =====================================================

bool masterKeyIsReady() {
  return masterKeyReady;
}

const uint8_t* masterKeyGetBytes() {
  if (!masterKeyReady) {
    return nullptr;
  }

  return masterKey;
}

size_t masterKeyGetLength() {
  return sizeof(masterKey);
}