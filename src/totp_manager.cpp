#include "totp_manager.h"

#include <WiFi.h>
#include <time.h>
#include "mbedtls/md.h"

// =====================================================
// Demo-TOTP Secret
// WICHTIG: Für echtes Produkt nicht im Code hardcoden.
// Später im verschlüsselten Vault speichern.
// Demo-Secret: "Hello!" Base32 = JBSWY3DPEHPK3PXP
// =====================================================

static const char* TOTP_LABEL = "Google TOTP";
static const char* TOTP_SECRET_BASE32 = "JBSWY3DPEHPK3PXP";

static uint8_t totpSecret[64];
static size_t totpSecretLen = 0;

static const char* NTP_SERVER_1 = "pool.ntp.org";
static const char* NTP_SERVER_2 = "time.google.com";

// Deutschland: UTC+1 / Sommerzeit wird hier noch nicht automatisch behandelt.
// Für Demo reicht das. TOTP selbst benutzt Unix-Zeit, Offset ist dafür egal,
// aber configTime braucht Werte.
static const long GMT_OFFSET_SEC = 0;
static const int DAYLIGHT_OFFSET_SEC = 0;

// =====================================================
// Base32 Decode
// =====================================================

static int base32Value(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a';
  if (c >= '2' && c <= '7') return c - '2' + 26;
  return -1;
}

static size_t decodeBase32(const char* input, uint8_t* output, size_t maxOutputLen) {
  int buffer = 0;
  int bitsLeft = 0;
  size_t outLen = 0;

  while (*input) {
    char c = *input++;

    if (c == ' ' || c == '=' || c == '-') {
      continue;
    }

    int val = base32Value(c);
    if (val < 0) {
      continue;
    }

    buffer = (buffer << 5) | val;
    bitsLeft += 5;

    if (bitsLeft >= 8) {
      bitsLeft -= 8;

      if (outLen >= maxOutputLen) {
        return 0;
      }

      output[outLen++] = (buffer >> bitsLeft) & 0xFF;
    }
  }

  return outLen;
}

// =====================================================
// TOTP Initialisierung
// =====================================================

bool totpBegin() {
  totpSecretLen = decodeBase32(TOTP_SECRET_BASE32, totpSecret, sizeof(totpSecret));
  return totpSecretLen > 0;
}

// =====================================================
// Zeit mit Wi-Fi/NTP synchronisieren
// =====================================================

bool totpSyncTimeWithWiFi(const char* ssid, const char* password) {
  if (ssid == nullptr || password == nullptr) {
    return false;
  }

  if (strlen(ssid) == 0 || strlen(password) == 0) {
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(200);

  WiFi.begin(ssid, password);

  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(250);
  }

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return false;
  }

  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER_1, NTP_SERVER_2);

  struct tm timeinfo;
  start = millis();

  while (millis() - start < 20000) {
    if (getLocalTime(&timeinfo)) {
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      return true;
    }

    delay(250);
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  return false;
}

// =====================================================
// Prüfen, ob Unix-Zeit plausibel ist
// =====================================================

bool totpIsTimeValid() {
  time_t now;
  time(&now);

  // Nach 2023 ungefähr gültig
  return now > 1700000000;
}

// =====================================================
// Aktuellen TOTP-Code berechnen
// =====================================================

uint32_t totpGetCurrentCode() {
  if (!totpIsTimeValid() || totpSecretLen == 0) {
    return 0;
  }

  time_t now;
  time(&now);

  uint64_t counter = ((uint64_t)now) / 30;

  uint8_t counterBytes[8];

  for (int i = 7; i >= 0; i--) {
    counterBytes[i] = counter & 0xFF;
    counter >>= 8;
  }

  uint8_t hmacResult[20];

  mbedtls_md_context_t ctx;
  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, info, 1);
  mbedtls_md_hmac_starts(&ctx, totpSecret, totpSecretLen);
  mbedtls_md_hmac_update(&ctx, counterBytes, 8);
  mbedtls_md_hmac_finish(&ctx, hmacResult);
  mbedtls_md_free(&ctx);

  int offset = hmacResult[19] & 0x0F;

  uint32_t binary =
    ((hmacResult[offset] & 0x7F) << 24) |
    ((hmacResult[offset + 1] & 0xFF) << 16) |
    ((hmacResult[offset + 2] & 0xFF) << 8) |
    (hmacResult[offset + 3] & 0xFF);

  return binary % 1000000;
}

// =====================================================
// Sekunden bis zum nächsten Code
// =====================================================

int totpGetSecondsRemaining() {
  time_t now;
  time(&now);

  int remaining = 30 - (now % 30);

  if (remaining == 30) {
    remaining = 0;
  }

  return remaining;
}

const char* totpGetLabel() {
  return TOTP_LABEL;
}
