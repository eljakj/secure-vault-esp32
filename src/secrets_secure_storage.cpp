#include "secrets_secure_storage.h"

#include "vault_file.h"
#include "vault_crypto.h"
#include "master_key.h"

#include "wifi_settings.h"
#include "totp_settings.h"

// =====================================================
// Datei-Pfad
// =====================================================

static const char* SECRETS_ENCRYPTED_PATH = "/secrets.enc";

// =====================================================
// Hilfsfunktion: Text escapen
// Format ist zeilenbasiert:
// key=value
// =====================================================

static String escapeValue(const char* input) {
  String output = "";

  if (input == nullptr) {
    return output;
  }

  for (int i = 0; input[i] != '\0'; i++) {
    char c = input[i];

    if (c == '\\') {
      output += "\\\\";
    } else if (c == '\n') {
      output += "\\n";
    } else if (c == '\r') {
      // ignorieren
    } else {
      output += c;
    }
  }

  return output;
}

static String unescapeValue(const String& input) {
  String output = "";

  for (int i = 0; i < input.length(); i++) {
    char c = input[i];

    if (c == '\\' && i + 1 < input.length()) {
      char next = input[i + 1];

      if (next == '\\') {
        output += '\\';
        i++;
      } else if (next == 'n') {
        output += '\n';
        i++;
      } else {
        output += next;
        i++;
      }
    } else {
      output += c;
    }
  }

  return output;
}

// =====================================================
// Secrets zu Text serialisieren
// =====================================================

static String secretsSerializeToText() {
  String output = "";

  output += "SECUREVAULT_SECRETS_V1\n";

  output += "wifi_ssid=";
  output += escapeValue(wifiSettingsGetSsid());
  output += "\n";

  output += "wifi_password=";
  output += escapeValue(wifiSettingsGetPassword());
  output += "\n";

  output += "totp_label=";
  output += escapeValue(totpSettingsGetLabel());
  output += "\n";

  output += "totp_secret=";
  output += escapeValue(totpSettingsGetBase32Secret());
  output += "\n";

  return output;
}

// =====================================================
// Eine key=value Zeile lesen
// =====================================================

static bool readValueByKey(const String& data, const char* key, String& valueOut) {
  valueOut = "";

  if (key == nullptr) {
    return false;
  }

  String prefix = String(key) + "=";

  int start = 0;

  while (start < data.length()) {
    int end = data.indexOf('\n', start);

    if (end < 0) {
      end = data.length();
    }

    String line = data.substring(start, end);

    if (line.startsWith(prefix)) {
      valueOut = unescapeValue(line.substring(prefix.length()));
      return true;
    }

    start = end + 1;
  }

  return false;
}

// =====================================================
// Text zurück in RAM laden
// =====================================================

static bool secretsDeserializeFromText(const String& data) {
  int firstLineEnd = data.indexOf('\n');

  if (firstLineEnd < 0) {
    return false;
  }

  String magic = data.substring(0, firstLineEnd);
  magic.trim();

  if (magic != "SECUREVAULT_SECRETS_V1") {
    return false;
  }

  String wifiSsid = "";
  String wifiPassword = "";
  String totpLabel = "";
  String totpSecret = "";

  readValueByKey(data, "wifi_ssid", wifiSsid);
  readValueByKey(data, "wifi_password", wifiPassword);
  readValueByKey(data, "totp_label", totpLabel);
  readValueByKey(data, "totp_secret", totpSecret);

  wifiSettingsSetRam(wifiSsid.c_str(), wifiPassword.c_str());
  totpSettingsSetRam(totpLabel.c_str(), totpSecret.c_str());

  return true;
}

// =====================================================
// Prüfen, ob verschlüsselte Secrets-Datei existiert
// =====================================================

bool secretsSecureStorageExists() {
  return vaultFileExists(SECRETS_ENCRYPTED_PATH);
}

// =====================================================
// RAM Secrets -> Text -> AES -> /secrets.enc
// =====================================================

bool secretsSecureStorageSave() {
  if (!masterKeyIsReady()) {
    Serial.println("secretsSecureStorageSave: master key not ready");
    return false;
  }

  String plainSecrets = secretsSerializeToText();
  String encryptedSecrets = "";

  bool encryptOk = vaultCryptoEncryptText(plainSecrets, encryptedSecrets);

  if (!encryptOk) {
    Serial.println("secretsSecureStorageSave: encrypt failed");
    return false;
  }

  bool writeOk = vaultFileWriteText(SECRETS_ENCRYPTED_PATH, encryptedSecrets);

  if (!writeOk) {
    Serial.println("secretsSecureStorageSave: file write failed");
    return false;
  }

  return true;
}

// =====================================================
// /secrets.enc -> AES decrypt -> Text -> RAM
// =====================================================

bool secretsSecureStorageLoad() {
  if (!masterKeyIsReady()) {
    Serial.println("secretsSecureStorageLoad: master key not ready");
    return false;
  }

  if (!secretsSecureStorageExists()) {
    return false;
  }

  String encryptedSecrets = "";
  bool readOk = vaultFileReadText(SECRETS_ENCRYPTED_PATH, encryptedSecrets);

  if (!readOk) {
    Serial.println("secretsSecureStorageLoad: file read failed");
    return false;
  }

  String plainSecrets = "";
  bool decryptOk = vaultCryptoDecryptText(encryptedSecrets, plainSecrets);

  if (!decryptOk) {
    Serial.println("secretsSecureStorageLoad: decrypt failed");
    return false;
  }

  bool parseOk = secretsDeserializeFromText(plainSecrets);

  if (!parseOk) {
    Serial.println("secretsSecureStorageLoad: deserialize failed");
    return false;
  }

  return true;
}

// =====================================================
// Verschlüsselte Secrets-Datei löschen
// =====================================================

bool secretsSecureStorageRemove() {
  return vaultFileRemove(SECRETS_ENCRYPTED_PATH);
}

// =====================================================
// Debug
// =====================================================

void secretsSecureStorageDebugPrint() {
  Serial.println();
  Serial.println("=== Secrets Secure Storage ===");

  Serial.print("Encrypted secrets file exists: ");
  Serial.println(secretsSecureStorageExists() ? "YES" : "NO");

  Serial.print("Master key ready: ");
  Serial.println(masterKeyIsReady() ? "YES" : "NO");

  Serial.println("==============================");
}