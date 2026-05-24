#include "vault_crypto.h"

#include "master_key.h"

#include "mbedtls/aes.h"

#include <esp_system.h>

// =====================================================
// Format:
// SECUREVAULT_AES_V1
// IV_HEX
// CIPHER_HEX
// =====================================================

static const char* CRYPTO_MAGIC = "SECUREVAULT_AES_V1";
static const size_t AES_BLOCK_SIZE_BYTES = 16;

// =====================================================
// Hex Helfer
// =====================================================

static char nibbleToHex(uint8_t value) {
  value &= 0x0F;

  if (value < 10) {
    return '0' + value;
  }

  return 'a' + (value - 10);
}

static String bytesToHex(const uint8_t* data, size_t length) {
  String output = "";
  output.reserve(length * 2);

  for (size_t i = 0; i < length; i++) {
    output += nibbleToHex(data[i] >> 4);
    output += nibbleToHex(data[i]);
  }

  return output;
}

static int hexValue(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

static bool hexToBytes(const String& hex, uint8_t* output, size_t outputLength) {
  if (hex.length() != outputLength * 2) {
    return false;
  }

  for (size_t i = 0; i < outputLength; i++) {
    int high = hexValue(hex[i * 2]);
    int low = hexValue(hex[(i * 2) + 1]);

    if (high < 0 || low < 0) {
      return false;
    }

    output[i] = (high << 4) | low;
  }

  return true;
}

// =====================================================
// Secure Clear
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
// Random IV erzeugen
// =====================================================

static void fillRandomBytes(uint8_t* output, size_t length) {
  for (size_t i = 0; i < length; i++) {
    output[i] = (uint8_t)(esp_random() & 0xFF);
  }
}

// =====================================================
// PKCS#7 Padding
// =====================================================

static size_t calculatePaddedLength(size_t inputLength) {
  size_t remainder = inputLength % AES_BLOCK_SIZE_BYTES;
  size_t paddingLength = AES_BLOCK_SIZE_BYTES - remainder;

  if (paddingLength == 0) {
    paddingLength = AES_BLOCK_SIZE_BYTES;
  }

  return inputLength + paddingLength;
}

static bool addPkcs7Padding(const uint8_t* input, size_t inputLength, uint8_t* output,
                            size_t outputLength) {
  if (outputLength == 0 || outputLength % AES_BLOCK_SIZE_BYTES != 0) {
    return false;
  }

  memcpy(output, input, inputLength);

  uint8_t paddingValue = outputLength - inputLength;

  for (size_t i = inputLength; i < outputLength; i++) {
    output[i] = paddingValue;
  }

  return true;
}

static bool removePkcs7Padding(uint8_t* data, size_t dataLength, size_t& plainLength) {
  if (dataLength == 0 || dataLength % AES_BLOCK_SIZE_BYTES != 0) {
    return false;
  }

  uint8_t paddingValue = data[dataLength - 1];

  if (paddingValue == 0 || paddingValue > AES_BLOCK_SIZE_BYTES) {
    return false;
  }

  if (paddingValue > dataLength) {
    return false;
  }

  for (size_t i = dataLength - paddingValue; i < dataLength; i++) {
    if (data[i] != paddingValue) {
      return false;
    }
  }

  plainLength = dataLength - paddingValue;
  return true;
}

// =====================================================
// AES Encrypt
// =====================================================

bool vaultCryptoEncryptText(const String& plainText, String& encryptedOutput) {
  encryptedOutput = "";

  if (!masterKeyIsReady()) {
    return false;
  }

  const uint8_t* key = masterKeyGetBytes();

  if (key == nullptr || masterKeyGetLength() != 32) {
    return false;
  }

  size_t plainLength = plainText.length();
  size_t paddedLength = calculatePaddedLength(plainLength);

  uint8_t* paddedBuffer = (uint8_t*)malloc(paddedLength);
  uint8_t* encryptedBuffer = (uint8_t*)malloc(paddedLength);

  if (paddedBuffer == nullptr || encryptedBuffer == nullptr) {
    if (paddedBuffer != nullptr)
      free(paddedBuffer);
    if (encryptedBuffer != nullptr)
      free(encryptedBuffer);
    return false;
  }

  bool paddingOk =
      addPkcs7Padding((const uint8_t*)plainText.c_str(), plainLength, paddedBuffer, paddedLength);

  if (!paddingOk) {
    secureClear(paddedBuffer, paddedLength);
    free(paddedBuffer);
    free(encryptedBuffer);
    return false;
  }

  uint8_t iv[16];
  fillRandomBytes(iv, sizeof(iv));

  uint8_t ivWorking[16];
  memcpy(ivWorking, iv, sizeof(iv));

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);

  int keyOk = mbedtls_aes_setkey_enc(&aes, key, 256);

  if (keyOk != 0) {
    mbedtls_aes_free(&aes);
    secureClear(paddedBuffer, paddedLength);
    secureClear(encryptedBuffer, paddedLength);
    free(paddedBuffer);
    free(encryptedBuffer);
    return false;
  }

  int cryptOk = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, paddedLength, ivWorking,
                                      paddedBuffer, encryptedBuffer);

  mbedtls_aes_free(&aes);

  if (cryptOk != 0) {
    secureClear(paddedBuffer, paddedLength);
    secureClear(encryptedBuffer, paddedLength);
    free(paddedBuffer);
    free(encryptedBuffer);
    return false;
  }

  encryptedOutput += CRYPTO_MAGIC;
  encryptedOutput += "\n";
  encryptedOutput += bytesToHex(iv, sizeof(iv));
  encryptedOutput += "\n";
  encryptedOutput += bytesToHex(encryptedBuffer, paddedLength);
  encryptedOutput += "\n";

  secureClear(paddedBuffer, paddedLength);
  secureClear(encryptedBuffer, paddedLength);
  secureClear(ivWorking, sizeof(ivWorking));

  free(paddedBuffer);
  free(encryptedBuffer);

  return true;
}

// =====================================================
// AES Decrypt
// =====================================================

bool vaultCryptoDecryptText(const String& encryptedInput, String& plainTextOutput) {
  plainTextOutput = "";

  if (!masterKeyIsReady()) {
    return false;
  }

  const uint8_t* key = masterKeyGetBytes();

  if (key == nullptr || masterKeyGetLength() != 32) {
    return false;
  }

  int firstLineEnd = encryptedInput.indexOf('\n');

  if (firstLineEnd < 0) {
    return false;
  }

  String magic = encryptedInput.substring(0, firstLineEnd);
  magic.trim();

  if (magic != CRYPTO_MAGIC) {
    return false;
  }

  int secondLineEnd = encryptedInput.indexOf('\n', firstLineEnd + 1);

  if (secondLineEnd < 0) {
    return false;
  }

  String ivHex = encryptedInput.substring(firstLineEnd + 1, secondLineEnd);
  ivHex.trim();

  int thirdLineEnd = encryptedInput.indexOf('\n', secondLineEnd + 1);

  if (thirdLineEnd < 0) {
    thirdLineEnd = encryptedInput.length();
  }

  String cipherHex = encryptedInput.substring(secondLineEnd + 1, thirdLineEnd);
  cipherHex.trim();

  if (ivHex.length() != 32) {
    return false;
  }

  if (cipherHex.length() == 0 || cipherHex.length() % 32 != 0) {
    return false;
  }

  uint8_t iv[16];

  if (!hexToBytes(ivHex, iv, sizeof(iv))) {
    return false;
  }

  size_t cipherLength = cipherHex.length() / 2;

  uint8_t* cipherBuffer = (uint8_t*)malloc(cipherLength);
  uint8_t* plainBuffer = (uint8_t*)malloc(cipherLength);

  if (cipherBuffer == nullptr || plainBuffer == nullptr) {
    if (cipherBuffer != nullptr)
      free(cipherBuffer);
    if (plainBuffer != nullptr)
      free(plainBuffer);
    return false;
  }

  if (!hexToBytes(cipherHex, cipherBuffer, cipherLength)) {
    secureClear(cipherBuffer, cipherLength);
    free(cipherBuffer);
    free(plainBuffer);
    return false;
  }

  uint8_t ivWorking[16];
  memcpy(ivWorking, iv, sizeof(iv));

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);

  int keyOk = mbedtls_aes_setkey_dec(&aes, key, 256);

  if (keyOk != 0) {
    mbedtls_aes_free(&aes);
    secureClear(cipherBuffer, cipherLength);
    secureClear(plainBuffer, cipherLength);
    free(cipherBuffer);
    free(plainBuffer);
    return false;
  }

  int cryptOk = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, cipherLength, ivWorking,
                                      cipherBuffer, plainBuffer);

  mbedtls_aes_free(&aes);

  if (cryptOk != 0) {
    secureClear(cipherBuffer, cipherLength);
    secureClear(plainBuffer, cipherLength);
    free(cipherBuffer);
    free(plainBuffer);
    return false;
  }

  size_t plainLength = 0;

  if (!removePkcs7Padding(plainBuffer, cipherLength, plainLength)) {
    secureClear(cipherBuffer, cipherLength);
    secureClear(plainBuffer, cipherLength);
    free(cipherBuffer);
    free(plainBuffer);
    return false;
  }

  plainTextOutput.reserve(plainLength);

  for (size_t i = 0; i < plainLength; i++) {
    plainTextOutput += (char)plainBuffer[i];
  }

  secureClear(cipherBuffer, cipherLength);
  secureClear(plainBuffer, cipherLength);
  secureClear(ivWorking, sizeof(ivWorking));

  free(cipherBuffer);
  free(plainBuffer);

  return true;
}

// =====================================================
// Debug Test
// =====================================================

void vaultCryptoDebugTest() {
  Serial.println();
  Serial.println("=== Vault Crypto Debug Test ===");

  if (!masterKeyIsReady()) {
    Serial.println("Master key: NOT READY");
    Serial.println("===============================");
    return;
  }

  String plain = "SECUREVAULT_TEST\nhello=world\n";
  String encrypted = "";
  String decrypted = "";

  bool encOk = vaultCryptoEncryptText(plain, encrypted);
  bool decOk = false;

  if (encOk) {
    decOk = vaultCryptoDecryptText(encrypted, decrypted);
  }

  Serial.print("Encrypt: ");
  Serial.println(encOk ? "OK" : "FAILED");

  Serial.print("Decrypt: ");
  Serial.println(decOk ? "OK" : "FAILED");

  Serial.print("Encrypted bytes: ");
  Serial.println(encrypted.length());

  Serial.print("Compare: ");
  Serial.println((encOk && decOk && plain == decrypted) ? "MATCH" : "DIFFERENT");

  Serial.println("===============================");
}