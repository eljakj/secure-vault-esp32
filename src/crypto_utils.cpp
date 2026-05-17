#include "crypto_utils.h"
#include "mbedtls/sha256.h"

// =====================================================
// PIN mit SHA-256 hashen
// =====================================================

void hashPin(const char* pin, char* outputHash) {
  byte hash[32];

  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, (const unsigned char*)pin, strlen(pin));
  mbedtls_sha256_finish(&ctx, hash);
  mbedtls_sha256_free(&ctx);

  for (int i = 0; i < 32; i++) {
    sprintf(outputHash + (i * 2), "%02x", hash[i]);
  }

  outputHash[64] = '\0';
}
