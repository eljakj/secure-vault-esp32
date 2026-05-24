#include <TFT_eSPI.h>
#include <Preferences.h>

#include "config.h"
#include "buttons.h"
#include "crypto_utils.h"
#include "storage.h"
#include "vault_model.h"
#include "web_portal.h"
#include "totp_manager.h"
#include "totp_settings.h"
#include "hid_manager.h"
#include "password_generator.h"
#include "wifi_settings.h"
#include "master_key.h"
#include "vault_file.h"
#include "vault_serializer.h"
#include "vault_crypto.h"
#include "vault_secure_storage.h"

// =====================================================
// Globale Objekte
// =====================================================

TFT_eSPI tft = TFT_eSPI();
Preferences preferences;

// =====================================================
// Screens
// =====================================================

enum ScreenState {
  SCREEN_LOCK,
  SCREEN_MENU,
  SCREEN_DETAIL,
  SCREEN_SETTINGS_HOME,
  SCREEN_CHANGE_PIN,
  SCREEN_FACTORY_RESET,
  SCREEN_WEB_PORTAL,
  SCREEN_TOTP_VIEW,
  SCREEN_PASSWORD_GENERATOR,
  SCREEN_VAULT_HOME,
  SCREEN_VAULT_LIST,
  SCREEN_VAULT_DETAIL,
  SCREEN_SEND_MENU,
  SCREEN_SENDING,
  SCREEN_SEARCH_LETTER,
  SCREEN_SEARCH_RESULTS,
  SCREEN_FAVORITES_LIST,
  SCREEN_RECENT_LIST
};

ScreenState currentScreen = SCREEN_LOCK;
ScreenState previousVaultListScreen = SCREEN_VAULT_LIST;

// =====================================================
// PIN / Security State
// =====================================================

char storedPinHash[65] = "";

char enteredPin[5] = "";
int enteredPinLength = 0;
int currentPinDigit = 0;
bool deviceUnlocked = false;

char newPin[5] = "";
int newPinLength = 0;
int currentNewPinDigit = 0;
bool oldPinVerifiedForChange = false;

int failedPinAttempts = 0;
bool pinLockoutActive = false;
unsigned long pinLockoutStartTime = 0;
unsigned long lastLockoutRedrawTime = 0;

unsigned long lastUserActivityTime = 0;

// =====================================================
// Menü-Daten
// =====================================================

const char* menuItems[] = {"Vault", "TOTP", "Add", "Settings", "Lock"};
const char* menuSubtitles[] = {"Passwords", "2FA codes", "New entry", "Device config",
                               "Secure device"};

const int menuCount = 5;
int selectedMenuItem = 0;
int menuScrollOffset = 0;
const int visibleMenuRows = 4;

// =====================================================
// Settings
// =====================================================

const char* settingsItems[] = {"Web Portal", "Change PIN",    "Auto Lock",
                               "Keyboard",   "Factory Reset", "About"};
const char* settingsSubtitles[] = {"Manage vault", "Set new PIN",  "60 sec",
                                   "US / DE",      "Erase device", "Device info"};

const int settingsCount = 6;
int selectedSettingsItem = 0;
int settingsScrollOffset = 0;
const int visibleSettingsRows = 4;

// =====================================================
// Factory Reset
// =====================================================

int factoryResetConfirmStep = 0;

// =====================================================
// Web Portal Sicherheit
// =====================================================

// Portal stoppt nach 10 Minuten ohne Aktivität
const unsigned long WEB_PORTAL_TIMEOUT_MS = 10UL * 60UL * 1000UL;

// =====================================================
// Keyboard Layout speichern/laden
// =====================================================

void loadKeyboardLayoutFromFlash() {
  preferences.begin("settings", true);
  int layoutValue = preferences.getInt("kbdLayout", KEYBOARD_LAYOUT_US);
  preferences.end();

  if (layoutValue == KEYBOARD_LAYOUT_DE) {
    hidSetKeyboardLayout(KEYBOARD_LAYOUT_DE);
  } else {
    hidSetKeyboardLayout(KEYBOARD_LAYOUT_US);
  }
}

void saveKeyboardLayoutToFlash() {
  preferences.begin("settings", false);
  preferences.putInt("kbdLayout", (int)hidGetKeyboardLayout());
  preferences.end();
}

// =====================================================
// Passwort-Generator
// =====================================================

int generatorLengthOptions[] = {12, 16, 20, 24, 32};
const int generatorLengthOptionCount = 5;
int selectedGeneratorLengthIndex = 1;  // Standard: 16 Zeichen
char generatedPassword[65] = "";
bool generatedPasswordReady = false;

// =====================================================
// Vault Home
// =====================================================

const char* vaultHomeItems[] = {"All", "Search", "Favorites", "Recent"};
const char* vaultHomeSubtitles[] = {"All entries", "Find entry", "Pinned items", "Last opened"};

const int vaultHomeCount = 4;
int selectedVaultHomeItem = 0;

// =====================================================
// Send-Menü für Auto-Type per USB HID
// =====================================================

const char* sendMenuItems[] = {"Username", "Password", "User+Tab+Pass", "Details"};

const char* sendMenuSubtitles[] = {"Type username", "Type password", "Login form", "View entry"};

const int sendMenuCount = 4;
int selectedSendMenuItem = 0;

// =====================================================
// Vault-Daten liegen jetzt in vault_model.cpp als VaultEntry[]
// =====================================================

int selectedVaultItem = 0;
int vaultScrollOffset = 0;
const int visibleVaultRows = 4;

bool passwordVisible = false;

// =====================================================
// Search
// =====================================================

char searchLetter = 'A';
int searchResults[20];
int searchResultCount = 0;
int selectedSearchResult = 0;
int searchScrollOffset = 0;

// =====================================================
// Favorites / Recent
// =====================================================

const int maxFavoriteCount = 10;
int favoriteIndexes[maxFavoriteCount] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
int favoriteCount = 0;
int selectedFavoriteItem = 0;
int favoriteScrollOffset = 0;

const int maxRecentCount = 5;
int recentIndexes[maxRecentCount] = {-1, -1, -1, -1, -1};
int recentCount = 0;
int selectedRecentItem = 0;
int recentScrollOffset = 0;

// =====================================================
// Forward declarations
// =====================================================

void drawMenu();
void drawLockScreen(const char* message = "");
void drawSettingsHome();
void drawChangePinScreen(const char* message = "");
void drawPasswordGenerator();
void drawFactoryResetScreen();
void drawWebPortalScreen(const char* message = "");
void drawVaultHome();
void drawVaultList();
void drawVaultDetail();
void drawSendMenu();
void drawSendingScreen(const char* title, const char* subtitle);
void drawSearchLetterScreen();
void drawSearchResults();
void drawFavoritesList();
void drawRecentList();
void goBack();

void addToRecent(int vaultIndex);

// =====================================================
// Grundfunktionen
// =====================================================

void setupDisplayPower() {
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);

  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, HIGH);

  tft.init();
  tft.setRotation(0);
}

void markUserActivity() {
  lastUserActivityTime = millis();
}

void resetPinInput() {
  enteredPinLength = 0;
  currentPinDigit = 0;

  for (int i = 0; i < 5; i++) {
    enteredPin[i] = '\0';
  }
}

void resetNewPinInput() {
  newPinLength = 0;
  currentNewPinDigit = 0;

  for (int i = 0; i < 5; i++) {
    newPin[i] = '\0';
  }
}

void resetChangePinFlow() {
  resetNewPinInput();
  oldPinVerifiedForChange = false;
}

// =====================================================
// UI Basis
// =====================================================

void drawStatusBar() {
  tft.fillRect(0, 0, SCREEN_WIDTH, 20, COLOR_BG);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_MUTED, COLOR_BG);

  tft.setCursor(8, 6);
  tft.print("USB");

  tft.setCursor(118, 6);
  tft.print("v0.1");

  tft.drawLine(0, 20, SCREEN_WIDTH, 20, COLOR_LINE);
}

void drawFooter(const char* leftText, const char* rightText) {
  tft.fillRect(0, 292, SCREEN_WIDTH, 28, COLOR_BG);
  tft.drawLine(0, 292, SCREEN_WIDTH, 292, COLOR_LINE);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_MUTED, COLOR_BG);

  tft.setCursor(8, 302);
  tft.print(leftText);

  tft.setCursor(92, 302);
  tft.print(rightText);
}

void drawHeader(const char* title, const char* subtitle) {
  tft.fillScreen(COLOR_BG);

  drawStatusBar();

  tft.setTextColor(COLOR_ACCENT, COLOR_BG);
  tft.setTextSize(2);
  tft.setCursor(10, 34);
  tft.println(title);

  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 58);
  tft.println(subtitle);

  tft.drawLine(10, 76, 160, 76, COLOR_LINE);
}

// =====================================================
// Lock / PIN
// =====================================================

void lockDevice() {
  webPortalStop();
  masterKeyClear();

  deviceUnlocked = false;
  passwordVisible = false;

  resetPinInput();
  drawLockScreen();
}

void drawLockScreen(const char* message) {
  currentScreen = SCREEN_LOCK;

  tft.fillScreen(COLOR_BG);
  drawStatusBar();

  tft.setTextColor(COLOR_ACCENT, COLOR_BG);
  tft.setTextSize(2);
  tft.setCursor(10, 40);
  tft.println("SecureVault");

  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextSize(2);
  tft.setCursor(10, 85);
  tft.println("LOCKED");

  if (pinLockoutActive) {
    unsigned long elapsed = millis() - pinLockoutStartTime;
    unsigned long remaining = 0;

    if (elapsed < PIN_LOCKOUT_MS) {
      remaining = (PIN_LOCKOUT_MS - elapsed) / 1000;
    }

    tft.setTextColor(TFT_RED, COLOR_BG);
    tft.setTextSize(1);
    tft.setCursor(10, 125);
    tft.println("Too many wrong tries");

    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextSize(2);
    tft.setCursor(10, 155);
    tft.print("Wait ");
    tft.print(remaining);
    tft.println("s");

    drawFooter("WAIT", "LOCKED");
    return;
  }

  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 125);
  tft.println("Enter PIN");

  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextSize(3);
  tft.setCursor(10, 155);

  for (int i = 0; i < 4; i++) {
    if (i < enteredPinLength) {
      tft.print("*");
    } else if (i == enteredPinLength) {
      tft.print(currentPinDigit);
    } else {
      tft.print("_");
    }

    tft.print(" ");
  }

  if (message != nullptr && message[0] != '\0') {
    tft.setTextColor(TFT_RED, COLOR_BG);
    tft.setTextSize(1);
    tft.setCursor(10, 225);
    tft.println(message);
  }

  if (failedPinAttempts > 0) {
    tft.setTextColor(COLOR_MUTED, COLOR_BG);
    tft.setTextSize(1);
    tft.setCursor(10, 240);
    tft.print("Wrong tries: ");
    tft.print(failedPinAttempts);
    tft.print("/");
    tft.print(MAX_PIN_ATTEMPTS);
  }

  drawFooter("DOWN +", "SELECT OK");
}

// =====================================================
// Encrypted Vault-Datei Test
// Testet:
// Vault -> serialize -> encrypt -> file write
// file read -> decrypt -> compare
// =====================================================

void testEncryptedVaultFileRoundTrip() {
  Serial.println();
  Serial.println("=== Encrypted Vault File Test ===");

  if (!masterKeyIsReady()) {
    Serial.println("Master key: NOT READY");
    Serial.println("=================================");
    return;
  }

  String plainVault = vaultSerializeToText();
  String encryptedVault = "";
  String loadedEncryptedVault = "";
  String decryptedVault = "";

  bool encryptOk = vaultCryptoEncryptText(plainVault, encryptedVault);

  Serial.print("Encrypt vault: ");
  Serial.println(encryptOk ? "OK" : "FAILED");

  if (!encryptOk) {
    Serial.println("=================================");
    return;
  }

  bool writeOk = vaultFileWriteText("/vault_test.enc", encryptedVault);

  Serial.print("Write /vault_test.enc: ");
  Serial.println(writeOk ? "OK" : "FAILED");

  if (!writeOk) {
    Serial.println("=================================");
    return;
  }

  bool readOk = vaultFileReadText("/vault_test.enc", loadedEncryptedVault);

  Serial.print("Read /vault_test.enc: ");
  Serial.println(readOk ? "OK" : "FAILED");

  if (!readOk) {
    Serial.println("=================================");
    return;
  }

  bool decryptOk = vaultCryptoDecryptText(loadedEncryptedVault, decryptedVault);

  Serial.print("Decrypt vault: ");
  Serial.println(decryptOk ? "OK" : "FAILED");

  if (!decryptOk) {
    Serial.println("=================================");
    return;
  }

  Serial.print("Plain bytes: ");
  Serial.println(plainVault.length());

  Serial.print("Encrypted bytes: ");
  Serial.println(encryptedVault.length());

  Serial.print("Loaded encrypted bytes: ");
  Serial.println(loadedEncryptedVault.length());

  bool same = plainVault == decryptedVault;

  Serial.print("Compare decrypted: ");
  Serial.println(same ? "MATCH" : "DIFFERENT");

  Serial.println("=================================");
}

void checkPin() {
  enteredPin[4] = '\0';

  char enteredPinHash[65];
  hashPin(enteredPin, enteredPinHash);

  if (strcmp(enteredPinHash, storedPinHash) == 0) {
    masterKeyDeriveFromPin(enteredPin);

    vaultSecureStorageDebugPrint();

    if (vaultSecureStorageExists()) {
      bool loadedEncryptedVault = vaultSecureStorageLoad();

      Serial.print("Load encrypted vault: ");
      Serial.println(loadedEncryptedVault ? "OK" : "FAILED");

      if (loadedEncryptedVault) {
        vaultClearPlaintextFlash();
      } else {
        vaultRestoreDefaultEntries();
      }
    } else {
      vaultRestoreDefaultEntries();

      bool savedEncryptedVault = vaultSecureStorageSave();

      Serial.print("Create encrypted vault: ");
      Serial.println(savedEncryptedVault ? "OK" : "FAILED");
    }

    vaultSerializerDebugPrint();

    deviceUnlocked = true;

    failedPinAttempts = 0;
    pinLockoutActive = false;

    resetPinInput();
    markUserActivity();

    drawMenu();
  } else {
    failedPinAttempts++;
    resetPinInput();

    if (failedPinAttempts >= MAX_PIN_ATTEMPTS) {
      pinLockoutActive = true;
      pinLockoutStartTime = millis();
      lastLockoutRedrawTime = 0;
      drawLockScreen();
    } else {
      char message[24];
      sprintf(message, "Wrong PIN %d/%d", failedPinAttempts, MAX_PIN_ATTEMPTS);
      drawLockScreen(message);
    }
  }
}

void checkAutoLock() {
  if (currentScreen == SCREEN_LOCK)
    return;
  if (!deviceUnlocked)
    return;

  // Während das Web Portal läuft, nicht automatisch sperren.
  // Sonst wird das WLAN mitten beim Bearbeiten getrennt.
  if (webPortalIsActive())
    return;

  if (millis() - lastUserActivityTime >= AUTO_LOCK_MS) {
    lockDevice();
  }
}

void checkPinLockout() {
  if (!pinLockoutActive)
    return;

  unsigned long now = millis();

  if (now - lastLockoutRedrawTime >= 1000) {
    lastLockoutRedrawTime = now;
    drawLockScreen();
  }

  if (now - pinLockoutStartTime >= PIN_LOCKOUT_MS) {
    pinLockoutActive = false;
    failedPinAttempts = 0;
    resetPinInput();
    drawLockScreen();
  }
}

// =====================================================
// Menü
// =====================================================

void updateMenuScrollOffset() {
  if (selectedMenuItem < menuScrollOffset) {
    menuScrollOffset = selectedMenuItem;
  }

  if (selectedMenuItem >= menuScrollOffset + visibleMenuRows) {
    menuScrollOffset = selectedMenuItem - visibleMenuRows + 1;
  }
}

void drawMenuRow(int index, int y, bool selected) {
  if (selected) {
    tft.fillRoundRect(8, y - 5, 154, 40, 7, COLOR_SELECTED);
    tft.setTextColor(COLOR_TEXT, COLOR_SELECTED);
  } else {
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
  }

  tft.setTextSize(2);
  tft.setCursor(16, y);
  tft.println(menuItems[index]);

  if (selected) {
    tft.setTextColor(TFT_LIGHTGREY, COLOR_SELECTED);
  } else {
    tft.setTextColor(COLOR_MUTED, COLOR_BG);
  }

  tft.setTextSize(1);
  tft.setCursor(16, y + 22);
  tft.println(menuSubtitles[index]);
}

void drawMenu() {
  currentScreen = SCREEN_MENU;

  updateMenuScrollOffset();

#if SECUREVAULT_DEV_MODE == 1
  drawHeader("SecureVault", "DEV mode - HID off");
#else
  drawHeader("SecureVault", "PRODUCT mode - HID on");
#endif

  int startY = 88;
  int itemHeight = 50;

  for (int row = 0; row < visibleMenuRows; row++) {
    int itemIndex = menuScrollOffset + row;

    if (itemIndex >= menuCount)
      break;

    int y = startY + row * itemHeight;
    bool selected = (itemIndex == selectedMenuItem);

    drawMenuRow(itemIndex, y, selected);
  }

  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 275);
  tft.print(selectedMenuItem + 1);
  tft.print("/");
  tft.print(menuCount);

  drawFooter("DOWN", "OPEN");
}

void drawGenericDetailScreen() {
  currentScreen = SCREEN_DETAIL;

  drawHeader(menuItems[selectedMenuItem], menuSubtitles[selectedMenuItem]);

  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 105);

  if (selectedMenuItem == 1) {
    tft.println("TOTP codes will");
    tft.println("appear here later.");
  } else if (selectedMenuItem == 2) {
    tft.println("Here we will add");
    tft.println("new entries later.");
  }

  drawFooter("HOLD SEL", "BACK");
}

// =====================================================
// Settings
// =====================================================

void drawSettingsRow(int index, int y, bool selected) {
  if (selected) {
    tft.fillRoundRect(8, y - 5, 154, 40, 7, COLOR_SELECTED);
    tft.setTextColor(COLOR_TEXT, COLOR_SELECTED);
  } else {
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
  }

  tft.setTextSize(2);
  tft.setCursor(16, y);
  tft.println(settingsItems[index]);

  if (selected) {
    tft.setTextColor(TFT_LIGHTGREY, COLOR_SELECTED);
  } else {
    tft.setTextColor(COLOR_MUTED, COLOR_BG);
  }

  tft.setTextSize(1);
  tft.setCursor(16, y + 22);
  tft.println(settingsSubtitles[index]);
}

// =====================================================
// Scroll-Position des Settings-Menüs anpassen
// =====================================================

void updateSettingsScrollOffset() {
  if (selectedSettingsItem < settingsScrollOffset) {
    settingsScrollOffset = selectedSettingsItem;
  }

  if (selectedSettingsItem >= settingsScrollOffset + visibleSettingsRows) {
    settingsScrollOffset = selectedSettingsItem - visibleSettingsRows + 1;
  }

  if (settingsScrollOffset < 0) {
    settingsScrollOffset = 0;
  }
}

void drawSettingsHome() {
  currentScreen = SCREEN_SETTINGS_HOME;

  updateSettingsScrollOffset();

  drawHeader("Settings", "Device config");

  int startY = 88;
  int itemHeight = 50;

  for (int row = 0; row < visibleSettingsRows; row++) {
    int itemIndex = settingsScrollOffset + row;

    if (itemIndex >= settingsCount) {
      break;
    }

    int y = startY + row * itemHeight;
    bool selected = (itemIndex == selectedSettingsItem);

    drawSettingsRow(itemIndex, y, selected);
  }

  // Kleine Scroll-Hinweise
  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  tft.setTextSize(1);

  if (settingsScrollOffset > 0) {
    tft.setCursor(150, 82);
    tft.print("^");
  }

  if (settingsScrollOffset + visibleSettingsRows < settingsCount) {
    tft.setCursor(150, 275);
    tft.print("v");
  }

  tft.setCursor(10, 275);
  tft.print(selectedSettingsItem + 1);
  tft.print("/");
  tft.print(settingsCount);

  drawFooter("DOWN", "OPEN");
}

void drawChangePinScreen(const char* message) {
  currentScreen = SCREEN_CHANGE_PIN;

  if (oldPinVerifiedForChange) {
    drawHeader("Change PIN", "Enter new PIN");
  } else {
    drawHeader("Change PIN", "Enter current PIN");
  }

  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 105);

  if (oldPinVerifiedForChange) {
    tft.println("Choose new 4 digits");
  } else {
    tft.println("Confirm your old PIN");
  }

  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextSize(3);
  tft.setCursor(10, 145);

  for (int i = 0; i < 4; i++) {
    if (i < newPinLength) {
      tft.print("*");
    } else if (i == newPinLength) {
      tft.print(currentNewPinDigit);
    } else {
      tft.print("_");
    }

    tft.print(" ");
  }

  if (message != nullptr && message[0] != '\0') {
    tft.setTextColor(COLOR_ACCENT, COLOR_BG);
    tft.setTextSize(1);
    tft.setCursor(10, 225);
    tft.println(message);
  }

  drawFooter("DOWN +", "SELECT OK");
}

bool verifyCurrentPinForChange() {
  newPin[4] = '\0';

  char enteredHash[65];
  hashPin(newPin, enteredHash);

  return strcmp(enteredHash, storedPinHash) == 0;
}

void saveNewPin() {
  newPin[4] = '\0';

  char newPinHash[65];
  hashPin(newPin, newPinHash);

  savePinHashToFlash(newPinHash, storedPinHash);

  resetChangePinFlow();

  drawHeader("PIN Saved", "Security updated");

  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 110);
  tft.println("Your new PIN");
  tft.println("has been saved.");

  drawFooter("HOLD SEL", "BACK");

  currentScreen = SCREEN_DETAIL;
}

// =====================================================
// Send-Menü-Zeile zeichnen
// =====================================================

void drawSendMenuRow(int index, int y, bool selected) {
  if (selected) {
    tft.fillRoundRect(8, y - 5, 154, 40, 7, COLOR_SELECTED);
    tft.setTextColor(COLOR_TEXT, COLOR_SELECTED);
  } else {
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
  }

  tft.setTextSize(2);
  tft.setCursor(16, y);
  tft.println(sendMenuItems[index]);

  if (selected) {
    tft.setTextColor(TFT_LIGHTGREY, COLOR_SELECTED);
  } else {
    tft.setTextColor(COLOR_MUTED, COLOR_BG);
  }

  tft.setTextSize(1);
  tft.setCursor(16, y + 22);
  tft.println(sendMenuSubtitles[index]);
}

// =====================================================
// Send-Menü zeichnen
// =====================================================

void drawSendMenu() {
  currentScreen = SCREEN_SEND_MENU;

  drawHeader(vaultGetTitle(selectedVaultItem), "Auto-Type action");

  int startY = 92;
  int itemHeight = 50;

  for (int i = 0; i < sendMenuCount; i++) {
    int y = startY + i * itemHeight;
    bool selected = (i == selectedSendMenuItem);

    drawSendMenuRow(i, y, selected);
  }

  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 275);
  tft.print(selectedSendMenuItem + 1);
  tft.print("/");
  tft.print(sendMenuCount);

  drawFooter("DOWN", "OPEN");
}

// =====================================================
// Sending-Screen zeichnen
// =====================================================

void drawSendingScreen(const char* title, const char* subtitle) {
  currentScreen = SCREEN_SENDING;

  drawHeader("Sending", title);

  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 110);
  tft.println(subtitle);

  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  tft.setCursor(10, 150);

  if (hidIsEnabled()) {
    tft.println("Check the target");
    tft.println("input field on PC.");
  } else {
    tft.println("DEV_MODE is active.");
    tft.println("HID typing disabled.");
    tft.println("Set DEV_MODE to 0");
    tft.println("in config.h.");
  }

  drawFooter("HOLD SEL", "BACK");
}

// =====================================================
// Auto-Type Aktion ausführen
// =====================================================

void performSendAction() {
  const char* username = vaultGetUsername(selectedVaultItem);
  const char* password = vaultGetPassword(selectedVaultItem);

  if (selectedSendMenuItem == 0) {
    drawSendingScreen("Username", username);
    delay(250);
    hidSendUsername(username);
  } else if (selectedSendMenuItem == 1) {
    drawSendingScreen("Password", "Password typed");
    delay(250);
    hidSendPassword(password);
  } else if (selectedSendMenuItem == 2) {
    drawSendingScreen("User+Tab+Pass", "Login typed");
    delay(250);
    hidSendUsernameTabPassword(username, password);
  } else if (selectedSendMenuItem == 3) {
    passwordVisible = false;
    selectedSendMenuItem = 0;
    drawSendMenu();
    return;
  }

  addToRecent(selectedVaultItem);
}

// =====================================================
// Passwort-Generator zeichnen
// =====================================================

void drawPasswordGenerator() {
  currentScreen = SCREEN_PASSWORD_GENERATOR;

  drawHeader("Generator", "Strong password");

  int lengthValue = generatorLengthOptions[selectedGeneratorLengthIndex];

  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 105);
  tft.println("Choose length with DOWN");

  tft.setTextColor(COLOR_ACCENT, COLOR_BG);
  tft.setTextSize(4);
  tft.setCursor(45, 140);
  tft.print(lengthValue);

  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 210);
  tft.println("SELECT generates");
  tft.println("a new password.");

  tft.setCursor(10, 250);
  tft.print("Keyboard: ");
  tft.println(hidGetKeyboardLayoutName());

  drawFooter("LENGTH", "GENERATE");
}

// =====================================================
// Generiertes Passwort anzeigen
// =====================================================

void drawGeneratedPasswordScreen() {
  currentScreen = SCREEN_PASSWORD_GENERATOR;

  drawHeader("Generated", "New password");

  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 100);
  tft.println("Password:");

  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 125);

  // Auf kleinem Display in 2 Zeilen umbrechen
  int len = strlen(generatedPassword);
  for (int i = 0; i < len; i++) {
    tft.print(generatedPassword[i]);

    if (i == 18) {
      tft.setCursor(10, 145);
    }
  }

  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  tft.setCursor(10, 205);
  tft.println("SELECT = type via USB");
  tft.println("DOWN = new length");

  drawFooter("LENGTH", "TYPE");
}

// =====================================================
// Keyboard Layout Info zeichnen
// =====================================================

void drawKeyboardLayoutScreen() {
  drawHeader("Keyboard", "USB HID layout");

  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 110);

  tft.print("Current layout: ");
  tft.println(hidGetKeyboardLayoutName());

  tft.setCursor(10, 145);
  if (hidGetKeyboardLayout() == KEYBOARD_LAYOUT_US) {
    tft.println("Use this when your");
    tft.println("PC layout is US.");
  } else {
    tft.println("Use this when your");
    tft.println("PC layout is DE.");
  }

  tft.setCursor(10, 210);
  tft.println("SELECT in Settings");
  tft.println("toggles US / DE.");

  drawFooter("HOLD SEL", "BACK");
  currentScreen = SCREEN_DETAIL;
}

// =====================================================
// TOTP Screen zeichnen
// =====================================================

void drawTotpView(bool syncing, const char* message) {
  currentScreen = SCREEN_TOTP_VIEW;

  drawHeader("TOTP", totpGetLabel());

  if (!totpHasSecret()) {
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextSize(1);
    tft.setCursor(10, 110);
    tft.println("No TOTP secret saved.");

    tft.setTextColor(COLOR_MUTED, COLOR_BG);
    tft.setCursor(10, 150);
    tft.println("Use Web Portal");
    tft.println("to add a TOTP");
    tft.println("Base32 secret.");

    drawFooter("WEB PORTAL", "BACK");
    return;
  }

  if (syncing) {
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextSize(1);
    tft.setCursor(10, 110);
    tft.println("Syncing time over");
    tft.println("Wi-Fi / NTP...");

    drawFooter("WAIT", "SYNC");
    return;
  }

  if (message != nullptr && message[0] != '\0') {
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextSize(1);
    tft.setCursor(10, 110);
    tft.println(message);

    tft.setTextColor(COLOR_MUTED, COLOR_BG);
    tft.setCursor(10, 160);
    tft.println("DOWN = sync time");
    tft.println("SELECT = type code");
    tft.println("HOLD SELECT = back");

    drawFooter("DOWN SYNC", "SELECT");
    return;
  }

  if (!totpIsTimeValid()) {
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextSize(1);
    tft.setCursor(10, 110);
    tft.println("Time not synced.");

    tft.setTextColor(COLOR_MUTED, COLOR_BG);
    tft.setCursor(10, 150);
    tft.println("Press DOWN to sync");
    tft.println("over Wi-Fi.");

    drawFooter("DOWN SYNC", "WAIT");
    return;
  }

  uint32_t code = totpGetCurrentCode();
  int remaining = totpGetSecondsRemaining();

  char codeText[8];
  sprintf(codeText, "%06lu", (unsigned long)code);

  tft.setTextColor(COLOR_ACCENT, COLOR_BG);
  tft.setTextSize(4);
  tft.setCursor(25, 120);
  tft.println(codeText);

  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 190);
  tft.print("Refresh in ");
  tft.print(remaining);
  tft.println("s");

  int barWidth = 140;
  int progress = (remaining * barWidth) / 30;
  tft.drawRect(10, 220, barWidth, 10, COLOR_LINE);
  tft.fillRect(10, 220, progress, 10, COLOR_ACCENT);

  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  tft.setCursor(10, 250);
  tft.println("SELECT types code via USB");

  drawFooter("DOWN SYNC", "TYPE");
}

// =====================================================
// TOTP-Code per USB-HID tippen
// =====================================================

void typeCurrentTotpCode() {
  if (!totpIsTimeValid()) {
    drawTotpView(false, "Time not synced.");
    return;
  }

  uint32_t code = totpGetCurrentCode();

  char codeText[8];
  sprintf(codeText, "%06lu", (unsigned long)code);

  drawSendingScreen("TOTP Code", "2FA code typed");
  delay(250);

  hidSendText(codeText);

  delay(300);
  drawTotpView(false, "");
}

// =====================================================
// TOTP Zeit synchronisieren
// =====================================================

void syncTotpTime() {
  drawTotpView(true, "");

  bool ok = false;

  if (wifiSettingsHasCredentials()) {
    ok = totpSyncTimeWithWiFi(wifiSettingsGetSsid(), wifiSettingsGetPassword());
  }

  if (ok) {
    drawTotpView(false, "");
  } else {
    drawTotpView(false, "Wi-Fi sync failed.");
  }
}

// =====================================================
// Web Portal Screen
// =====================================================

void drawWebPortalScreen(const char* message) {
  currentScreen = SCREEN_WEB_PORTAL;

  drawHeader("Web Portal", "Local management");

  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 105);

  if (!webPortalIsActive()) {
    tft.println("Portal is off.");
    tft.println("");
    tft.println("SELECT = start");
    tft.println("HOLD SELECT = back");

    if (message != nullptr && message[0] != '\0') {
      tft.setTextColor(TFT_RED, COLOR_BG);
      tft.setCursor(10, 190);
      tft.println(message);
    }

    drawFooter("SELECT", "START");
    return;
  }

  tft.println("Connect phone/PC to:");

  tft.setTextColor(COLOR_ACCENT, COLOR_BG);
  tft.setCursor(10, 135);
  tft.print("SSID: ");
  tft.println(webPortalGetSsid());

  tft.setCursor(10, 160);
  tft.println("PASS:");

  const char* portalPass = webPortalGetPassword();

  tft.setTextSize(1);
  tft.setCursor(10, 175);

  for (int i = 0; portalPass[i] != '\0'; i++) {
    tft.print(portalPass[i]);

    if (i == 7) {
      tft.println();
      tft.setCursor(10, 190);
    } else if (i == 3 || i == 11) {
      tft.print("-");
    }
  }

  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(10, 215);
  tft.print("Open: ");
  tft.println(webPortalGetIp());

  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  tft.setCursor(10, 240);
  tft.println("http://192.168.4.1");

  tft.setCursor(10, 260);
  if (hidIsEnabled()) {
    tft.println("SELECT types password");
    tft.println("Auto-stop: 10 min");
  } else {
    tft.println("DEV_MODE: HID typing off");
  }

  drawFooter("TYPE PASS", "HOLD STOP");
}

// =====================================================
// Web Portal starten
// =====================================================

void startWebPortalFromUi() {
  drawWebPortalScreen("Starting...");

  webPortalSetPinHash(storedPinHash);

  bool ok = webPortalStart();

  if (ok) {
    webPortalMarkActivity();
    drawWebPortalScreen();
  } else {
    drawWebPortalScreen("AP start failed.");
  }
}

// =====================================================
// Factory Reset Screen
// =====================================================

void drawFactoryResetScreen() {
  currentScreen = SCREEN_FACTORY_RESET;

  drawHeader("Factory Reset", "Erase stored data");

  tft.setTextColor(TFT_RED, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 105);
  tft.println("WARNING:");
  tft.println("This will erase:");
  tft.println("- saved PIN hash");
  tft.println("- favorites");
  tft.println("- recent list");
  tft.println("- settings");

  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(10, 185);

  if (factoryResetConfirmStep == 0) {
    tft.println("Press SELECT once");
    tft.println("to arm reset.");
  } else {
    tft.println("Reset armed.");
    tft.println("Press SELECT again");
    tft.println("to erase.");
  }

  drawFooter("SELECT", "HOLD BACK");
}

// =====================================================
// Factory Reset durchführen
// =====================================================

void performFactoryReset() {
  webPortalStop();
  clearAllStoredData();

  // Lokalen RAM-Zustand zurücksetzen
  strncpy(storedPinHash, DEFAULT_PIN_HASH, 65);

  // Vault auf Demo-Standard zurücksetzen
  vaultRestoreDefaultEntries();
  vaultSaveToFlash();

  for (int i = 0; i < maxFavoriteCount; i++) {
    favoriteIndexes[i] = -1;
  }

  favoriteCount = 0;
  selectedFavoriteItem = 0;
  favoriteScrollOffset = 0;

  for (int i = 0; i < maxRecentCount; i++) {
    recentIndexes[i] = -1;
  }

  recentCount = 0;
  selectedRecentItem = 0;
  recentScrollOffset = 0;

  selectedMenuItem = 0;
  menuScrollOffset = 0;
  selectedVaultHomeItem = 0;
  selectedSettingsItem = 0;
  settingsScrollOffset = 0;
  factoryResetConfirmStep = 0;

  failedPinAttempts = 0;
  pinLockoutActive = false;
  deviceUnlocked = false;
  passwordVisible = false;

  resetPinInput();
  resetChangePinFlow();

  drawHeader("Reset Done", "Device cleared");

  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 110);
  tft.println("Factory reset done.");
  tft.println("Default PIN is now:");
  tft.println("1234");

  tft.setCursor(10, 165);
  tft.println("Vault is empty.");
  tft.println("Use Web Portal");
  tft.println("to add entries.");

  tft.setCursor(10, 220);
  tft.println("Hold SELECT to lock.");

  drawFooter("HOLD SEL", "LOCK");

  currentScreen = SCREEN_DETAIL;
}

// =====================================================
// Vault UI
// =====================================================

void drawVaultHomeRow(int index, int y, bool selected) {
  if (selected) {
    tft.fillRoundRect(8, y - 5, 154, 40, 7, COLOR_SELECTED);
    tft.setTextColor(COLOR_TEXT, COLOR_SELECTED);
  } else {
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
  }

  tft.setTextSize(2);
  tft.setCursor(16, y);
  tft.println(vaultHomeItems[index]);

  if (selected) {
    tft.setTextColor(TFT_LIGHTGREY, COLOR_SELECTED);
  } else {
    tft.setTextColor(COLOR_MUTED, COLOR_BG);
  }

  tft.setTextSize(1);
  tft.setCursor(16, y + 22);
  tft.println(vaultHomeSubtitles[index]);
}

void drawVaultHome() {
  currentScreen = SCREEN_VAULT_HOME;

  drawHeader("Vault", "Choose mode");

  int startY = 92;
  int itemHeight = 50;

  for (int i = 0; i < vaultHomeCount; i++) {
    int y = startY + i * itemHeight;
    bool selected = (i == selectedVaultHomeItem);

    drawVaultHomeRow(i, y, selected);
  }

  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 275);
  tft.print(selectedVaultHomeItem + 1);
  tft.print("/");
  tft.print(vaultHomeCount);

  drawFooter("DOWN", "OPEN");
}

void drawVaultRow(int index, int y, bool selected) {
  if (selected) {
    tft.fillRoundRect(8, y - 6, 154, 44, 7, COLOR_SELECTED);
    tft.setTextColor(COLOR_TEXT, COLOR_SELECTED);
  } else {
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
  }

  tft.setTextSize(2);
  tft.setCursor(16, y);
  tft.println(vaultGetTitle(index));

  if (selected) {
    tft.setTextColor(TFT_LIGHTGREY, COLOR_SELECTED);
  } else {
    tft.setTextColor(COLOR_MUTED, COLOR_BG);
  }

  tft.setTextSize(1);
  tft.setCursor(16, y + 24);
  tft.println(vaultGetUsername(index));
}

void updateVaultScrollOffset() {
  if (selectedVaultItem < vaultScrollOffset) {
    vaultScrollOffset = selectedVaultItem;
  }

  if (selectedVaultItem >= vaultScrollOffset + visibleVaultRows) {
    vaultScrollOffset = selectedVaultItem - visibleVaultRows + 1;
  }
}

void drawVaultList() {
  currentScreen = SCREEN_VAULT_LIST;

  updateVaultScrollOffset();

  drawHeader("Vault", "Saved passwords");

  if (vaultCount == 0) {
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextSize(1);
    tft.setCursor(10, 110);
    tft.println("No entries yet.");

    tft.setTextColor(COLOR_MUTED, COLOR_BG);
    tft.setCursor(10, 150);
    tft.println("Use Settings >");
    tft.println("Web Portal to add");
    tft.println("passwords.");

    drawFooter("HOLD SEL", "BACK");
    return;
  }

  int startY = 88;
  int itemHeight = 50;

  for (int row = 0; row < visibleVaultRows; row++) {
    int itemIndex = vaultScrollOffset + row;

    if (itemIndex >= vaultCount)
      break;

    int y = startY + row * itemHeight;
    bool selected = (itemIndex == selectedVaultItem);

    drawVaultRow(itemIndex, y, selected);
  }

  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 275);
  tft.print(selectedVaultItem + 1);
  tft.print("/");
  tft.print(vaultCount);

  drawFooter("DOWN", "OPEN");
}

// =====================================================
// Favorites / Recent
// =====================================================

bool isFavorite(int vaultIndex) {
  for (int i = 0; i < favoriteCount; i++) {
    if (favoriteIndexes[i] == vaultIndex)
      return true;
  }

  return false;
}

void addToFavorites(int vaultIndex) {
  if (isFavorite(vaultIndex))
    return;
  if (favoriteCount >= maxFavoriteCount)
    return;

  favoriteIndexes[favoriteCount] = vaultIndex;
  favoriteCount++;

  selectedFavoriteItem = 0;
  favoriteScrollOffset = 0;

  saveFavoritesToFlash(favoriteIndexes, maxFavoriteCount, favoriteCount);
}

void removeFromFavorites(int vaultIndex) {
  int foundIndex = -1;

  for (int i = 0; i < favoriteCount; i++) {
    if (favoriteIndexes[i] == vaultIndex) {
      foundIndex = i;
      break;
    }
  }

  if (foundIndex == -1)
    return;

  for (int i = foundIndex; i < favoriteCount - 1; i++) {
    favoriteIndexes[i] = favoriteIndexes[i + 1];
  }

  favoriteIndexes[favoriteCount - 1] = -1;
  favoriteCount--;

  if (favoriteCount < 0)
    favoriteCount = 0;

  selectedFavoriteItem = 0;
  favoriteScrollOffset = 0;

  saveFavoritesToFlash(favoriteIndexes, maxFavoriteCount, favoriteCount);
}

void toggleFavorite(int vaultIndex) {
  if (isFavorite(vaultIndex)) {
    removeFromFavorites(vaultIndex);
  } else {
    addToFavorites(vaultIndex);
  }
}

void updateFavoriteScrollOffset() {
  if (selectedFavoriteItem < favoriteScrollOffset) {
    favoriteScrollOffset = selectedFavoriteItem;
  }

  if (selectedFavoriteItem >= favoriteScrollOffset + visibleVaultRows) {
    favoriteScrollOffset = selectedFavoriteItem - visibleVaultRows + 1;
  }
}

void updateRecentScrollOffset() {
  if (selectedRecentItem < recentScrollOffset) {
    recentScrollOffset = selectedRecentItem;
  }

  if (selectedRecentItem >= recentScrollOffset + visibleVaultRows) {
    recentScrollOffset = selectedRecentItem - visibleVaultRows + 1;
  }
}

void addToRecent(int vaultIndex) {
  int existingPosition = -1;

  for (int i = 0; i < recentCount; i++) {
    if (recentIndexes[i] == vaultIndex) {
      existingPosition = i;
      break;
    }
  }

  if (existingPosition != -1) {
    for (int i = existingPosition; i < recentCount - 1; i++) {
      recentIndexes[i] = recentIndexes[i + 1];
    }

    recentCount--;
  }

  if (recentCount >= maxRecentCount) {
    recentCount = maxRecentCount - 1;
  }

  for (int i = recentCount; i > 0; i--) {
    recentIndexes[i] = recentIndexes[i - 1];
  }

  recentIndexes[0] = vaultIndex;
  recentCount++;

  selectedRecentItem = 0;
  recentScrollOffset = 0;

  saveRecentToFlash(recentIndexes, maxRecentCount, recentCount);
}

void drawVaultDetail() {
  currentScreen = SCREEN_VAULT_DETAIL;

  drawHeader(vaultGetTitle(selectedVaultItem), "Entry details");

  tft.setTextSize(1);

  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  tft.setCursor(10, 105);
  tft.println("Username");

  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(10, 123);
  tft.println(vaultGetUsername(selectedVaultItem));

  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  tft.setCursor(10, 145);
  tft.print("Category: ");
  tft.println(vaultGetCategory(selectedVaultItem));

  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  tft.setCursor(10, 170);
  tft.println("Password");

  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(10, 188);

  if (passwordVisible) {
    tft.println(vaultGetPassword(selectedVaultItem));
  } else {
    tft.println("********");
  }

  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  tft.setCursor(10, 225);

  if (passwordVisible) {
    tft.println("Password is visible");
  } else {
    tft.println("Password is hidden");
  }

  tft.setCursor(10, 250);
  tft.print("Favorite: ");

  if (isFavorite(selectedVaultItem)) {
    tft.println("Yes");
  } else {
    tft.println("No");
  }

  drawFooter("FAV/SHOW", "HOLD BACK");
}

void drawFavoritesList() {
  currentScreen = SCREEN_FAVORITES_LIST;

  updateFavoriteScrollOffset();

  drawHeader("Favorites", "Pinned entries");

  if (favoriteCount == 0) {
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextSize(1);
    tft.setCursor(10, 110);
    tft.println("No favorites yet.");

    drawFooter("HOLD SEL", "BACK");
    return;
  }

  int startY = 88;
  int itemHeight = 50;

  for (int row = 0; row < visibleVaultRows; row++) {
    int favoriteListIndex = favoriteScrollOffset + row;

    if (favoriteListIndex >= favoriteCount)
      break;

    int vaultIndex = favoriteIndexes[favoriteListIndex];

    int y = startY + row * itemHeight;
    bool selected = (favoriteListIndex == selectedFavoriteItem);

    drawVaultRow(vaultIndex, y, selected);
  }

  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 275);
  tft.print(selectedFavoriteItem + 1);
  tft.print("/");
  tft.print(favoriteCount);

  drawFooter("DOWN", "OPEN");
}

void drawRecentList() {
  currentScreen = SCREEN_RECENT_LIST;

  updateRecentScrollOffset();

  drawHeader("Recent", "Last opened");

  if (recentCount == 0) {
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextSize(1);
    tft.setCursor(10, 110);
    tft.println("No recent entries.");

    drawFooter("HOLD SEL", "BACK");
    return;
  }

  int startY = 88;
  int itemHeight = 50;

  for (int row = 0; row < visibleVaultRows; row++) {
    int recentListIndex = recentScrollOffset + row;

    if (recentListIndex >= recentCount)
      break;

    int vaultIndex = recentIndexes[recentListIndex];

    int y = startY + row * itemHeight;
    bool selected = (recentListIndex == selectedRecentItem);

    drawVaultRow(vaultIndex, y, selected);
  }

  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 275);
  tft.print(selectedRecentItem + 1);
  tft.print("/");
  tft.print(recentCount);

  drawFooter("DOWN", "OPEN");
}

// =====================================================
// Search
// =====================================================

void buildSearchResults() {
  searchResultCount = 0;
  selectedSearchResult = 0;
  searchScrollOffset = 0;

  for (int i = 0; i < vaultCount; i++) {
    char firstChar = vaultGetTitle(i)[0];

    if (firstChar >= 'a' && firstChar <= 'z') {
      firstChar = firstChar - 32;
    }

    if (firstChar == searchLetter) {
      searchResults[searchResultCount] = i;
      searchResultCount++;
    }
  }
}

void updateSearchScrollOffset() {
  if (selectedSearchResult < searchScrollOffset) {
    searchScrollOffset = selectedSearchResult;
  }

  if (selectedSearchResult >= searchScrollOffset + visibleVaultRows) {
    searchScrollOffset = selectedSearchResult - visibleVaultRows + 1;
  }
}

void drawSearchLetterScreen() {
  currentScreen = SCREEN_SEARCH_LETTER;

  drawHeader("Search", "Choose first letter");

  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 100);
  tft.println("Search entries by");
  tft.println("first letter.");

  tft.setTextColor(COLOR_ACCENT, COLOR_BG);
  tft.setTextSize(6);
  tft.setCursor(65, 145);
  tft.print(searchLetter);

  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 260);
  tft.println("DOWN changes letter");

  drawFooter("DOWN", "SEARCH");
}

void drawSearchResults() {
  currentScreen = SCREEN_SEARCH_RESULTS;

  updateSearchScrollOffset();

  char subtitle[20];
  sprintf(subtitle, "Results for %c", searchLetter);

  drawHeader("Search", subtitle);

  if (searchResultCount == 0) {
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextSize(1);
    tft.setCursor(10, 110);
    tft.println("No entries found.");

    drawFooter("HOLD SEL", "BACK");
    return;
  }

  int startY = 88;
  int itemHeight = 50;

  for (int row = 0; row < visibleVaultRows; row++) {
    int resultIndex = searchScrollOffset + row;

    if (resultIndex >= searchResultCount)
      break;

    int vaultIndex = searchResults[resultIndex];

    int y = startY + row * itemHeight;
    bool selected = (resultIndex == selectedSearchResult);

    drawVaultRow(vaultIndex, y, selected);
  }

  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 275);
  tft.print(selectedSearchResult + 1);
  tft.print("/");
  tft.print(searchResultCount);

  drawFooter("DOWN", "OPEN");
}

// =====================================================
// Navigation
// =====================================================

void goBack() {
  if (currentScreen == SCREEN_MENU)
    return;

  if (currentScreen == SCREEN_SEND_MENU) {
    if (previousVaultListScreen == SCREEN_VAULT_LIST) {
      drawVaultList();
    } else if (previousVaultListScreen == SCREEN_SEARCH_RESULTS) {
      drawSearchResults();
    } else if (previousVaultListScreen == SCREEN_FAVORITES_LIST) {
      drawFavoritesList();
    } else if (previousVaultListScreen == SCREEN_RECENT_LIST) {
      drawRecentList();
    } else {
      drawVaultHome();
    }
    return;
  }

  if (currentScreen == SCREEN_SENDING) {
    drawSendMenu();
    return;
  }

  if (currentScreen == SCREEN_TOTP_VIEW) {
    drawMenu();
    return;
  }

  if (currentScreen == SCREEN_WEB_PORTAL) {
    webPortalStop();
    drawSettingsHome();
    return;
  }

  if (currentScreen == SCREEN_FACTORY_RESET) {
    factoryResetConfirmStep = 0;
    drawSettingsHome();
    return;
  }

  if (currentScreen == SCREEN_PASSWORD_GENERATOR) {
    generatedPasswordReady = false;
    drawMenu();
    return;
  }

  if (currentScreen == SCREEN_SETTINGS_HOME) {
    drawMenu();
    return;
  }

  if (currentScreen == SCREEN_CHANGE_PIN) {
    resetChangePinFlow();
    drawSettingsHome();
    return;
  }

  if (currentScreen == SCREEN_FAVORITES_LIST) {
    drawVaultHome();
    return;
  }

  if (currentScreen == SCREEN_RECENT_LIST) {
    drawVaultHome();
    return;
  }

  if (currentScreen == SCREEN_SEARCH_RESULTS) {
    drawSearchLetterScreen();
    return;
  }

  if (currentScreen == SCREEN_SEARCH_LETTER) {
    drawVaultHome();
    return;
  }

  if (currentScreen == SCREEN_VAULT_DETAIL) {
    passwordVisible = false;

    if (previousVaultListScreen == SCREEN_VAULT_LIST) {
      drawVaultList();
    } else if (previousVaultListScreen == SCREEN_SEARCH_RESULTS) {
      drawSearchResults();
    } else if (previousVaultListScreen == SCREEN_FAVORITES_LIST) {
      drawFavoritesList();
    } else if (previousVaultListScreen == SCREEN_RECENT_LIST) {
      drawRecentList();
    } else {
      drawVaultHome();
    }

    return;
  }

  if (currentScreen == SCREEN_VAULT_LIST) {
    drawVaultHome();
    return;
  }

  if (currentScreen == SCREEN_VAULT_HOME) {
    drawMenu();
    return;
  }

  if (currentScreen == SCREEN_DETAIL) {
    drawMenu();
    return;
  }
}

// =====================================================
// Input Handler
// =====================================================

void handleLockInput(ButtonEvent event) {
  if (pinLockoutActive)
    return;

  if (event == BUTTON_DOWN_SHORT) {
    currentPinDigit++;

    if (currentPinDigit > 9)
      currentPinDigit = 0;

    drawLockScreen();
  }

  if (event == BUTTON_SELECT_SHORT) {
    if (enteredPinLength < 4) {
      enteredPin[enteredPinLength] = '0' + currentPinDigit;
      enteredPinLength++;
      currentPinDigit = 0;
    }

    if (enteredPinLength >= 4) {
      checkPin();
    } else {
      drawLockScreen();
    }
  }

  if (event == BUTTON_SELECT_LONG) {
    resetPinInput();
    drawLockScreen();
  }
}

void handleMenuInput(ButtonEvent event) {
  if (event == BUTTON_DOWN_SHORT) {
    selectedMenuItem++;

    if (selectedMenuItem >= menuCount) {
      selectedMenuItem = 0;
      menuScrollOffset = 0;
    }

    drawMenu();
  }

  if (event == BUTTON_SELECT_SHORT) {
    if (selectedMenuItem == 0) {
      drawVaultHome();
    } else if (selectedMenuItem == 1) {
      drawTotpView(false, "");
    } else if (selectedMenuItem == 2) {
      generatedPasswordReady = false;
      drawPasswordGenerator();
    } else if (selectedMenuItem == 3) {
      drawSettingsHome();
    } else if (selectedMenuItem == 4) {
      lockDevice();
    } else {
      drawGenericDetailScreen();
    }
  }
}

void handleGenericDetailInput(ButtonEvent event) {
  if (event == BUTTON_SELECT_LONG) {
    goBack();
  }
}

void handleSettingsHomeInput(ButtonEvent event) {
  if (event == BUTTON_DOWN_SHORT) {
    selectedSettingsItem++;

    if (selectedSettingsItem >= settingsCount) {
      selectedSettingsItem = 0;
      settingsScrollOffset = 0;
    }

    drawSettingsHome();
  }

  if (event == BUTTON_SELECT_SHORT) {
    if (selectedSettingsItem == 0) {
      drawWebPortalScreen();
    } else if (selectedSettingsItem == 1) {
      resetChangePinFlow();
      drawChangePinScreen();
    } else if (selectedSettingsItem == 2) {
      drawHeader("Auto Lock", "Current setting");

      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      tft.setTextSize(1);
      tft.setCursor(10, 110);
      tft.println("Auto lock is");
      tft.println("currently 60 sec.");

      drawFooter("HOLD SEL", "BACK");
      currentScreen = SCREEN_DETAIL;
    } else if (selectedSettingsItem == 3) {
      hidToggleKeyboardLayout();
      saveKeyboardLayoutToFlash();
      drawKeyboardLayoutScreen();
    } else if (selectedSettingsItem == 4) {
      factoryResetConfirmStep = 0;
      drawFactoryResetScreen();
    } else if (selectedSettingsItem == 5) {
      drawHeader("About", "SecureVault");

      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      tft.setTextSize(1);
      tft.setCursor(10, 110);
      tft.println("SecureVault v0.1");
      tft.println("Master key build");
      tft.println("LILYGO T-Display-S3");

      tft.print("Keyboard: ");
      tft.println(hidGetKeyboardLayoutName());

      tft.print("Key ready: ");
      if (masterKeyIsReady()) {
        tft.println("YES");
      } else {
        tft.println("NO");
      }

      drawFooter("HOLD SEL", "BACK");
      currentScreen = SCREEN_DETAIL;
    }
  }

  if (event == BUTTON_SELECT_LONG) {
    goBack();
  }
}

void handleChangePinInput(ButtonEvent event) {
  if (event == BUTTON_DOWN_SHORT) {
    currentNewPinDigit++;

    if (currentNewPinDigit > 9)
      currentNewPinDigit = 0;

    drawChangePinScreen();
  }

  if (event == BUTTON_SELECT_SHORT) {
    if (newPinLength < 4) {
      newPin[newPinLength] = '0' + currentNewPinDigit;
      newPinLength++;
      currentNewPinDigit = 0;
    }

    if (newPinLength >= 4) {
      if (!oldPinVerifiedForChange) {
        if (verifyCurrentPinForChange()) {
          oldPinVerifiedForChange = true;
          resetNewPinInput();
          drawChangePinScreen("Old PIN OK");
        } else {
          resetNewPinInput();
          drawChangePinScreen("Wrong old PIN");
        }
      } else {
        saveNewPin();
      }
    } else {
      drawChangePinScreen();
    }
  }

  if (event == BUTTON_SELECT_LONG) {
    resetChangePinFlow();
    drawSettingsHome();
  }
}

void handleSendMenuInput(ButtonEvent event) {
  if (event == BUTTON_DOWN_SHORT) {
    selectedSendMenuItem++;

    if (selectedSendMenuItem >= sendMenuCount) {
      selectedSendMenuItem = 0;
    }

    drawSendMenu();
  }

  if (event == BUTTON_SELECT_SHORT) {
    performSendAction();
  }

  if (event == BUTTON_SELECT_LONG) {
    goBack();
  }
}

void handleSendingInput(ButtonEvent event) {
  if (event == BUTTON_SELECT_LONG) {
    goBack();
  }
}

void handleTotpInput(ButtonEvent event) {
  // DOWN kurz: Zeit neu synchronisieren
  if (event == BUTTON_DOWN_SHORT) {
    syncTotpTime();
  }

  // SELECT kurz:
  // - Wenn Zeit noch nicht gültig ist: synchronisieren
  // - Wenn Zeit gültig ist: aktuellen TOTP-Code per USB tippen
  if (event == BUTTON_SELECT_SHORT) {
    if (!totpIsTimeValid()) {
      syncTotpTime();
    } else {
      typeCurrentTotpCode();
    }
  }

  if (event == BUTTON_SELECT_LONG) {
    goBack();
  }
}

void handleWebPortalInput(ButtonEvent event) {
  webPortalMarkActivity();

  if (event == BUTTON_SELECT_SHORT) {
    if (!webPortalIsActive()) {
      startWebPortalFromUi();
    } else {
      drawSendingScreen("WiFi Password", "Portal password typed");
      delay(250);
      hidSendText(webPortalGetPassword());
      delay(300);
      drawWebPortalScreen();
    }
  }

  if (event == BUTTON_SELECT_LONG) {
    webPortalStop();
    drawSettingsHome();
  }
}

void handleFactoryResetInput(ButtonEvent event) {
  if (event == BUTTON_SELECT_SHORT) {
    if (factoryResetConfirmStep == 0) {
      factoryResetConfirmStep = 1;
      drawFactoryResetScreen();
    } else {
      performFactoryReset();
    }
  }

  if (event == BUTTON_SELECT_LONG) {
    factoryResetConfirmStep = 0;
    drawSettingsHome();
  }
}

void handlePasswordGeneratorInput(ButtonEvent event) {
  if (event == BUTTON_DOWN_SHORT) {
    selectedGeneratorLengthIndex++;

    if (selectedGeneratorLengthIndex >= generatorLengthOptionCount) {
      selectedGeneratorLengthIndex = 0;
    }

    generatedPasswordReady = false;
    drawPasswordGenerator();
  }

  if (event == BUTTON_SELECT_SHORT) {
    if (!generatedPasswordReady) {
      int lengthValue = generatorLengthOptions[selectedGeneratorLengthIndex];
      generateStrongPassword(generatedPassword, sizeof(generatedPassword), lengthValue);
      generatedPasswordReady = true;
      drawGeneratedPasswordScreen();
    } else {
      drawSendingScreen("Generated Pass", "Password typed");
      delay(250);
      hidSendPassword(generatedPassword);
      drawGeneratedPasswordScreen();
    }
  }

  if (event == BUTTON_SELECT_LONG) {
    goBack();
  }
}

void handleVaultHomeInput(ButtonEvent event) {
  if (event == BUTTON_DOWN_SHORT) {
    selectedVaultHomeItem++;

    if (selectedVaultHomeItem >= vaultHomeCount)
      selectedVaultHomeItem = 0;

    drawVaultHome();
  }

  if (event == BUTTON_SELECT_SHORT) {
    if (selectedVaultHomeItem == 0) {
      drawVaultList();
    } else if (selectedVaultHomeItem == 1) {
      drawSearchLetterScreen();
    } else if (selectedVaultHomeItem == 2) {
      drawFavoritesList();
    } else if (selectedVaultHomeItem == 3) {
      drawRecentList();
    }
  }

  if (event == BUTTON_SELECT_LONG) {
    goBack();
  }
}

void handleVaultListInput(ButtonEvent event) {
  if (vaultCount == 0) {
    if (event == BUTTON_SELECT_LONG) {
      goBack();
    }
    return;
  }

  if (event == BUTTON_DOWN_SHORT) {
    selectedVaultItem++;

    if (selectedVaultItem >= vaultCount) {
      selectedVaultItem = 0;
      vaultScrollOffset = 0;
    }

    drawVaultList();
  }

  if (event == BUTTON_SELECT_SHORT) {
    previousVaultListScreen = SCREEN_VAULT_LIST;

    addToRecent(selectedVaultItem);

    passwordVisible = false;
    selectedSendMenuItem = 0;
    drawSendMenu();
  }

  if (event == BUTTON_SELECT_LONG) {
    goBack();
  }
}

void handleVaultDetailInput(ButtonEvent event) {
  if (event == BUTTON_SELECT_SHORT) {
    passwordVisible = !passwordVisible;
    drawVaultDetail();
  }

  if (event == BUTTON_DOWN_SHORT) {
    toggleFavorite(selectedVaultItem);
    drawVaultDetail();
  }

  if (event == BUTTON_SELECT_LONG) {
    passwordVisible = false;
    goBack();
  }
}

void handleSearchLetterInput(ButtonEvent event) {
  if (event == BUTTON_DOWN_SHORT) {
    searchLetter++;

    if (searchLetter > 'Z')
      searchLetter = 'A';

    drawSearchLetterScreen();
  }

  if (event == BUTTON_SELECT_SHORT) {
    buildSearchResults();
    drawSearchResults();
  }

  if (event == BUTTON_SELECT_LONG) {
    goBack();
  }
}

void handleSearchResultsInput(ButtonEvent event) {
  if (event == BUTTON_DOWN_SHORT) {
    if (searchResultCount == 0)
      return;

    selectedSearchResult++;

    if (selectedSearchResult >= searchResultCount) {
      selectedSearchResult = 0;
      searchScrollOffset = 0;
    }

    drawSearchResults();
  }

  if (event == BUTTON_SELECT_SHORT) {
    if (searchResultCount == 0)
      return;

    int vaultIndex = searchResults[selectedSearchResult];

    selectedVaultItem = vaultIndex;
    previousVaultListScreen = SCREEN_SEARCH_RESULTS;

    addToRecent(vaultIndex);

    passwordVisible = false;
    selectedSendMenuItem = 0;
    drawSendMenu();
  }

  if (event == BUTTON_SELECT_LONG) {
    goBack();
  }
}

void handleFavoritesListInput(ButtonEvent event) {
  if (event == BUTTON_DOWN_SHORT) {
    selectedFavoriteItem++;

    if (selectedFavoriteItem >= favoriteCount) {
      selectedFavoriteItem = 0;
      favoriteScrollOffset = 0;
    }

    drawFavoritesList();
  }

  if (event == BUTTON_SELECT_SHORT) {
    if (favoriteCount == 0)
      return;

    int vaultIndex = favoriteIndexes[selectedFavoriteItem];

    selectedVaultItem = vaultIndex;
    previousVaultListScreen = SCREEN_FAVORITES_LIST;

    addToRecent(vaultIndex);

    passwordVisible = false;
    selectedSendMenuItem = 0;
    drawSendMenu();
  }

  if (event == BUTTON_SELECT_LONG) {
    goBack();
  }
}

void handleRecentListInput(ButtonEvent event) {
  if (event == BUTTON_DOWN_SHORT) {
    selectedRecentItem++;

    if (selectedRecentItem >= recentCount) {
      selectedRecentItem = 0;
      recentScrollOffset = 0;
    }

    drawRecentList();
  }

  if (event == BUTTON_SELECT_SHORT) {
    if (recentCount == 0)
      return;

    int vaultIndex = recentIndexes[selectedRecentItem];

    selectedVaultItem = vaultIndex;
    previousVaultListScreen = SCREEN_RECENT_LIST;

    addToRecent(vaultIndex);

    passwordVisible = false;
    selectedSendMenuItem = 0;
    drawSendMenu();
  }

  if (event == BUTTON_SELECT_LONG) {
    goBack();
  }
}

void handleButtons() {
  ButtonEvent event = readButtonEvent();

  if (event == BUTTON_NONE)
    return;

  markUserActivity();

  if (currentScreen == SCREEN_LOCK) {
    handleLockInput(event);
  } else if (currentScreen == SCREEN_MENU) {
    handleMenuInput(event);
  } else if (currentScreen == SCREEN_SETTINGS_HOME) {
    handleSettingsHomeInput(event);
  } else if (currentScreen == SCREEN_CHANGE_PIN) {
    handleChangePinInput(event);
  } else if (currentScreen == SCREEN_WEB_PORTAL) {
    handleWebPortalInput(event);
  } else if (currentScreen == SCREEN_FACTORY_RESET) {
    handleFactoryResetInput(event);
  } else if (currentScreen == SCREEN_TOTP_VIEW) {
    handleTotpInput(event);
  } else if (currentScreen == SCREEN_PASSWORD_GENERATOR) {
    handlePasswordGeneratorInput(event);
  } else if (currentScreen == SCREEN_DETAIL) {
    handleGenericDetailInput(event);
  } else if (currentScreen == SCREEN_VAULT_HOME) {
    handleVaultHomeInput(event);
  } else if (currentScreen == SCREEN_VAULT_LIST) {
    handleVaultListInput(event);
  } else if (currentScreen == SCREEN_VAULT_DETAIL) {
    handleVaultDetailInput(event);
  } else if (currentScreen == SCREEN_SEND_MENU) {
    handleSendMenuInput(event);
  } else if (currentScreen == SCREEN_SENDING) {
    handleSendingInput(event);
  } else if (currentScreen == SCREEN_SEARCH_LETTER) {
    handleSearchLetterInput(event);
  } else if (currentScreen == SCREEN_SEARCH_RESULTS) {
    handleSearchResultsInput(event);
  } else if (currentScreen == SCREEN_FAVORITES_LIST) {
    handleFavoritesListInput(event);
  } else if (currentScreen == SCREEN_RECENT_LIST) {
    handleRecentListInput(event);
  }
}

// =====================================================
// LittleFS Vault-Datei Test
// Noch keine Verschlüsselung.
// Testet nur: Serialisieren -> Datei schreiben -> lesen.
// =====================================================

void testVaultFileRoundTrip() {
  Serial.println();
  Serial.println("=== Vault File Roundtrip Test ===");

  String serializedVault = vaultSerializeToText();

  bool writeOk = vaultFileWriteText("/vault_test.txt", serializedVault);

  Serial.print("Write /vault_test.txt: ");
  Serial.println(writeOk ? "OK" : "FAILED");

  if (!writeOk) {
    Serial.println("===============================");
    return;
  }

  String loadedVault = "";
  bool readOk = vaultFileReadText("/vault_test.txt", loadedVault);

  Serial.print("Read /vault_test.txt: ");
  Serial.println(readOk ? "OK" : "FAILED");

  if (!readOk) {
    Serial.println("===============================");
    return;
  }

  Serial.print("Written bytes: ");
  Serial.println(serializedVault.length());

  Serial.print("Read bytes: ");
  Serial.println(loadedVault.length());

  bool same = serializedVault == loadedVault;

  Serial.print("Compare: ");
  Serial.println(same ? "MATCH" : "DIFFERENT");

  Serial.println("===============================");
}

// =====================================================
// Arduino Setup / Loop
// =====================================================

void setup() {
  masterKeyClear();

  Serial.begin(115200);
  delay(300);

  bool fsOk = vaultFileBegin();

  if (fsOk) {
    Serial.println("LittleFS started");
    vaultFileDebugPrintInfo();
  } else {
    Serial.println("LittleFS failed");
  }

  setupDisplayPower();
  buttonsBegin();
  hidBegin();
  loadKeyboardLayoutFromFlash();

  wifiSettingsLoad();
  totpSettingsLoad();
  totpBegin();

  loadPinHashFromFlash(storedPinHash);

  // Vault bleibt bis zum PIN-Unlock leer.
  // Nach erfolgreichem PIN wird /vault.enc geladen.
  vaultRestoreDefaultEntries();
  loadRecentFromFlash(recentIndexes, maxRecentCount, recentCount);
  loadFavoritesFromFlash(favoriteIndexes, maxFavoriteCount, favoriteCount);

  lastUserActivityTime = millis();

  resetPinInput();
  drawLockScreen();
}

// =====================================================
// Web Portal Auto-Stop prüfen
// =====================================================

void checkWebPortalTimeout() {
  if (!webPortalIsActive()) {
    return;
  }

  unsigned long now = millis();
  unsigned long lastActivity = webPortalGetLastActivityTime();

  if (now - lastActivity >= WEB_PORTAL_TIMEOUT_MS) {
    webPortalStop();

    if (currentScreen == SCREEN_WEB_PORTAL) {
      drawWebPortalScreen("Portal timed out.");
    }
  }
}

void loop() {
  webPortalProcess();
  checkWebPortalTimeout();

  handleButtons();
  checkAutoLock();
  checkPinLockout();

  static unsigned long lastTotpRedraw = 0;

  if (currentScreen == SCREEN_TOTP_VIEW && totpIsTimeValid()) {
    unsigned long now = millis();

    if (now - lastTotpRedraw >= 1000) {
      lastTotpRedraw = now;
      drawTotpView(false, "");
    }
  }
}
