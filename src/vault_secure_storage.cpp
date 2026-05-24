#include "vault_secure_storage.h"

#include "vault_file.h"
#include "vault_serializer.h"
#include "vault_crypto.h"
#include "master_key.h"

// =====================================================
// Datei-Pfad
// =====================================================

static const char* VAULT_ENCRYPTED_PATH = "/vault.enc";

// =====================================================
// Prüfen, ob verschlüsselte Vault-Datei existiert
// =====================================================

bool vaultSecureStorageExists() {
  return vaultFileExists(VAULT_ENCRYPTED_PATH);
}

// =====================================================
// Vault RAM -> Text -> AES -> /vault.enc
// =====================================================

bool vaultSecureStorageSave() {
  if (!masterKeyIsReady()) {
    Serial.println("vaultSecureStorageSave: master key not ready");
    return false;
  }

  String plainVault = vaultSerializeToText();
  String encryptedVault = "";

  bool encryptOk = vaultCryptoEncryptText(plainVault, encryptedVault);

  if (!encryptOk) {
    Serial.println("vaultSecureStorageSave: encrypt failed");
    return false;
  }

  bool writeOk = vaultFileWriteText(VAULT_ENCRYPTED_PATH, encryptedVault);

  if (!writeOk) {
    Serial.println("vaultSecureStorageSave: file write failed");
    return false;
  }

  return true;
}

// =====================================================
// /vault.enc -> AES decrypt -> Text -> Vault RAM
// =====================================================

bool vaultSecureStorageLoad() {
  if (!masterKeyIsReady()) {
    Serial.println("vaultSecureStorageLoad: master key not ready");
    return false;
  }

  if (!vaultSecureStorageExists()) {
    Serial.println("vaultSecureStorageLoad: no encrypted vault file");
    return false;
  }

  String encryptedVault = "";
  bool readOk = vaultFileReadText(VAULT_ENCRYPTED_PATH, encryptedVault);

  if (!readOk) {
    Serial.println("vaultSecureStorageLoad: file read failed");
    return false;
  }

  String plainVault = "";
  bool decryptOk = vaultCryptoDecryptText(encryptedVault, plainVault);

  if (!decryptOk) {
    Serial.println("vaultSecureStorageLoad: decrypt failed");
    return false;
  }

  bool parseOk = vaultDeserializeFromText(plainVault);

  if (!parseOk) {
    Serial.println("vaultSecureStorageLoad: deserialize failed");
    return false;
  }

  return true;
}

// =====================================================
// Verschlüsselte Datei löschen
// =====================================================

bool vaultSecureStorageRemove() {
  return vaultFileRemove(VAULT_ENCRYPTED_PATH);
}

// =====================================================
// Debug
// =====================================================

void vaultSecureStorageDebugPrint() {
  Serial.println();
  Serial.println("=== Vault Secure Storage ===");

  Serial.print("Encrypted file exists: ");
  Serial.println(vaultSecureStorageExists() ? "YES" : "NO");

  Serial.print("Master key ready: ");
  Serial.println(masterKeyIsReady() ? "YES" : "NO");

  Serial.println("============================");
}