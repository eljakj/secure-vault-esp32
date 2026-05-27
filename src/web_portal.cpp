#include "web_portal.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>

#include "vault_model.h"
#include "password_generator.h"
#include "wifi_settings.h"
#include "totp_settings.h"
#include "totp_manager.h"
#include "crypto_utils.h"
#include "secrets_secure_storage.h"
#include "debug_log.h"
#include "vault_file.h"
#include "vault_secure_storage.h"

extern Preferences preferences;

// =====================================================
// Lokales Web Portal
// Kein Cloud-Server. Alles läuft direkt auf dem ESP32.
// =====================================================

static WebServer server(80);
static DNSServer dnsServer;

static const byte DNS_PORT = 53;

static bool portalActive = false;
static String portalPassword = "";
static String portalIp = "192.168.4.1";
static unsigned long lastPortalActivityTime = 0;

// Portal Login
static bool portalAuthenticated = false;
static char portalPinHash[65] = "";

// Schutz gegen PIN-Bruteforce im Web Portal
static int portalFailedPinAttempts = 0;
static bool portalLoginLocked = false;
static unsigned long portalLoginLockedAt = 0;

static const int PORTAL_MAX_PIN_ATTEMPTS = 3;
static const unsigned long PORTAL_LOCKOUT_MS = 30000;

static const char* PORTAL_SSID = "SecureVault-Setup";

// =====================================================
// Web Portal Aktivität merken
// =====================================================

void webPortalMarkActivity() {
  lastPortalActivityTime = millis();
}

unsigned long webPortalGetLastActivityTime() {
  return lastPortalActivityTime;
}

// =====================================================
// Web Portal PIN/Login
// =====================================================

void webPortalSetPinHash(const char* pinHash) {
  if (pinHash == nullptr) {
    portalPinHash[0] = '\0';
    return;
  }

  strncpy(portalPinHash, pinHash, sizeof(portalPinHash) - 1);
  portalPinHash[sizeof(portalPinHash) - 1] = '\0';
}

void webPortalLogout() {
  portalAuthenticated = false;
}

// =====================================================
// Web Portal PIN Lockout
// =====================================================

static bool isPortalLockoutActive() {
  if (!portalLoginLocked) {
    return false;
  }

  unsigned long now = millis();

  if (now - portalLoginLockedAt >= PORTAL_LOCKOUT_MS) {
    portalLoginLocked = false;
    portalFailedPinAttempts = 0;
    return false;
  }

  return true;
}

static int getPortalLockoutSecondsRemaining() {
  if (!portalLoginLocked) {
    return 0;
  }

  unsigned long elapsed = millis() - portalLoginLockedAt;

  if (elapsed >= PORTAL_LOCKOUT_MS) {
    return 0;
  }

  return (PORTAL_LOCKOUT_MS - elapsed) / 1000;
}

static bool webPortalCheckPin(const String& pin) {
  if (pin.length() != 4) {
    return false;
  }

  if (strlen(portalPinHash) != 64) {
    return false;
  }

  char inputHash[65];
  hashPin(pin.c_str(), inputHash);

  return strcmp(inputHash, portalPinHash) == 0;
}

static void sendLoginPage(const char* message = "") {
  String html = "";
  html +=
      "<!doctype html><html><head><meta name='viewport' "
      "content='width=device-width,initial-scale=1'>";
  html += "<title>SecureVault Login</title>";
  html += "<style>";
  html +=
      "body{margin:0;background:#0b0b0b;color:#f5f5f5;font-family:Arial,sans-serif;padding:22px;}";
  html += ".wrap{max-width:420px;margin:0 auto;}";
  html +=
      ".card{background:#161616;border:1px solid "
      "#2a2a2a;border-radius:18px;padding:22px;margin-top:30px;}";
  html += "h1{text-align:center;font-size:26px;margin:0 0 18px;}";
  html += "label{display:block;margin:12px 0 6px;color:#d0d0d0;font-weight:700;}";
  html +=
      "input{width:100%;box-sizing:border-box;padding:14px;border-radius:12px;border:1px solid "
      "#333;background:#0e0e0e;color:#fff;font-size:20px;text-align:center;letter-spacing:8px;}";
  html +=
      ".btn{width:100%;border:0;border-radius:12px;padding:13px "
      "14px;background:#2979ff;color:#fff;text-decoration:none;font-weight:700;cursor:pointer;"
      "margin-top:16px;font-size:16px;}";
  html += ".muted{color:#aaa;font-size:14px;line-height:1.4;}";
  html += ".error{color:#ff6b6b;font-weight:700;}";
  html += "</style></head><body><div class='wrap'>";
  html += "<div class='card'>";
  html += "<h1>SecureVault</h1>";
  html += "<p class='muted'>Enter device PIN to unlock the local web portal.</p>";

  if (message != nullptr && message[0] != '\0') {
    html += "<p class='error'>";
    html += message;
    html += "</p>";
  }

  if (portalLoginLocked) {
    html += "<p class='error'>Too many wrong tries. Wait ";
    html += String(getPortalLockoutSecondsRemaining());
    html += " seconds.</p>";
  }

  html += "<form method='POST' action='/login_submit'>";
  html += "<label>PIN</label>";
  html += "<input name='pin' inputmode='numeric' maxlength='4' type='password' autofocus>";
  html += "<button class='btn' type='submit'>Unlock Web Portal</button>";
  html += "</form>";

  html += "<p class='muted'>No cloud. This page runs directly on the ESP32.</p>";
  html += "</div></div></body></html>";

  server.send(200, "text/html", html);
}

static bool requirePortalLogin() {
  if (portalAuthenticated) {
    return true;
  }

  sendLoginPage();
  return false;
}

static void handleLogin() {
  webPortalMarkActivity();
  sendLoginPage();
}

static void handleLoginSubmit() {
  webPortalMarkActivity();

  if (isPortalLockoutActive()) {
    sendLoginPage("Web portal is temporarily locked.");
    return;
  }

  String pin = server.arg("pin");
  pin.trim();

  if (webPortalCheckPin(pin)) {
    portalAuthenticated = true;
    portalFailedPinAttempts = 0;
    portalLoginLocked = false;

    server.sendHeader("Location", "/");
    server.send(303);
    return;
  }

  portalAuthenticated = false;
  portalFailedPinAttempts++;

  if (portalFailedPinAttempts >= PORTAL_MAX_PIN_ATTEMPTS) {
    portalLoginLocked = true;
    portalLoginLockedAt = millis();

    sendLoginPage("Too many wrong PIN attempts.");
    return;
  }

  String message = "Wrong PIN. Tries ";
  message += String(portalFailedPinAttempts);
  message += "/";
  message += String(PORTAL_MAX_PIN_ATTEMPTS);
  message += ".";

  sendLoginPage(message.c_str());
}

static void handleLogout() {
  webPortalMarkActivity();
  portalAuthenticated = false;
  sendLoginPage("Logged out.");
}

// =====================================================
// Kleine HTML-Helfer
// =====================================================

static String htmlEscape(const char* input) {
  String s = input ? input : "";
  s.replace("&", "&amp;");
  s.replace("<", "&lt;");
  s.replace(">", "&gt;");
  s.replace("\"", "&quot;");
  return s;
}

static String urlBack() {
  return "<p><a class='btn secondary' href='/'>Back</a></p>";
}

static String pageHeader(const String& title) {
  String html = "";

  html += "<!doctype html><html><head>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>SecureVault</title>";

  html += "<style>";

  html += "*{box-sizing:border-box;}";
  html += "body{";
  html += "margin:0;";
  html += "background:#0b0f17;";
  html += "color:#f5f7fb;";
  html += "font-family:Arial,sans-serif;";
  html += "padding:18px;";
  html += "}";

  html += ".wrap{max-width:980px;margin:0 auto;}";

  html += "h1{";
  html += "font-size:28px;";
  html += "margin:0 0 18px;";
  html += "text-align:center;";
  html += "}";

  html += "h2{";
  html += "font-size:20px;";
  html += "margin:0 0 14px;";
  html += "}";

  html += ".card{";
  html += "background:#151b26;";
  html += "border:1px solid #263244;";
  html += "border-radius:18px;";
  html += "padding:18px;";
  html += "margin-bottom:16px;";
  html += "}";

  html += "label{";
  html += "display:block;";
  html += "margin:12px 0 6px;";
  html += "color:#cbd5e1;";
  html += "font-weight:700;";
  html += "}";

  html += "input,select,textarea{";
  html += "width:100%;";
  html += "padding:13px;";
  html += "border-radius:12px;";
  html += "border:1px solid #334155;";
  html += "background:#0f172a;";
  html += "color:#fff;";
  html += "font-size:16px;";
  html += "}";

  html += "textarea{min-height:260px;font-family:monospace;font-size:12px;}";

  html += "table{width:100%;border-collapse:collapse;}";
  html += "th,td{";
  html += "border-bottom:1px solid #263244;";
  html += "padding:12px 8px;";
  html += "text-align:left;";
  html += "vertical-align:middle;";
  html += "}";

  html += "th{";
  html += "color:#94a3b8;";
  html += "font-size:13px;";
  html += "text-transform:uppercase;";
  html += "}";

  html += ".actions{";
  html += "display:flex;";
  html += "gap:8px;";
  html += "flex-wrap:wrap;";
  html += "margin-top:14px;";
  html += "}";

  html += ".btn{";
  html += "display:inline-block;";
  html += "border:0;";
  html += "border-radius:12px;";
  html += "padding:11px 14px;";
  html += "background:#2563eb;";
  html += "color:#fff;";
  html += "text-decoration:none;";
  html += "font-weight:700;";
  html += "cursor:pointer;";
  html += "}";

  html += ".secondary{background:#475569;}";
  html += ".danger{background:#dc2626;}";
  html += ".muted{color:#94a3b8;font-size:14px;line-height:1.45;}";

  html += ".top{";
  html += "display:flex;";
  html += "gap:10px;";
  html += "flex-wrap:wrap;";
  html += "justify-content:center;";
  html += "margin-bottom:16px;";
  html += "}";

  html += "@media(max-width:720px){";
  html += "body{padding:14px;}";
  html += ".card{padding:16px;}";
  html += ".btn{width:auto;}";
  html += ".actions .btn{flex:1 1 auto;text-align:center;}";
  html += "table{font-size:14px;}";
  html += "th,td{padding:10px 6px;}";
  html += "}";

  html += "</style></head><body><div class='wrap'>";

  html += "<h1>";
  html += title;
  html += "</h1>";

  html += "<div class='top'>";
  html += "<a class='btn' href='/'>Vault</a>";
  html += "<a class='btn' href='/add'>Add Entry</a>";
  html += "<a class='btn secondary' href='/totp'>TOTP</a>";
  html += "<a class='btn secondary' href='/wifi'>Wi-Fi</a>";
  html += "<a class='btn secondary' href='/info'>Info</a>";
  html += "<a class='btn danger' href='/logout'>Logout</a>";
  html += "</div>";

  return html;
}

static String pageFooter() {
  return "</div></body></html>";
}

// =====================================================
// Passwort für AP erzeugen
// =====================================================

static String generatePortalPassword() {
  char pass[17];
  generateStrongPassword(pass, sizeof(pass), 16);
  return String(pass);
}

// =====================================================
// Portal-Passwort dauerhaft laden
// Wichtig: Nicht bei jedem Portal-Start neu erzeugen,
// sonst muss Windows/Handy jedes Mal "Netzwerk vergessen".
// =====================================================

static String loadOrCreatePortalPassword() {
  preferences.begin("portal", false);

  String savedPassword = preferences.getString("apPass", "");

  if (savedPassword.length() >= 8 && savedPassword.length() <= 63) {
    preferences.end();
    return savedPassword;
  }

  String newPassword = generatePortalPassword();
  preferences.putString("apPass", newPassword);

  preferences.end();
  return newPassword;
}

// =====================================================
// Dashboard / Vault-Liste
// =====================================================

static void handleRoot() {
  webPortalMarkActivity();

  if (!requirePortalLogin()) {
    return;
  }

  String html = pageHeader("SecureVault Web Portal");

  html += "<div class='card'>";
  html += "<h2>Local Vault</h2>";
  html += "<p class='muted'>Manage entries locally on this ESP32. No cloud.</p>";

  if (vaultCount == 0) {
    html += "<p>No entries yet.</p>";
  } else {
    html += "<table><tr><th>Title</th><th>Username</th><th>Category</th><th>Actions</th></tr>";

    for (int i = 0; i < vaultCount; i++) {
      html += "<tr>";
      html += "<td>" + htmlEscape(vaultGetTitle(i)) + "</td>";
      html += "<td>" + htmlEscape(vaultGetUsername(i)) + "</td>";
      html += "<td>" + htmlEscape(vaultGetCategory(i)) + "</td>";
      html += "<td>";
      html += "<a class='btn secondary' href='/edit?i=" + String(i) + "'>Edit</a> ";
      html += "<a class='btn danger' href='/delete?i=" + String(i) + "'>Delete</a>";
      html += "</td>";
      html += "</tr>";
    }

    html += "</table>";
  }

  html += "</div>";
  html += pageFooter();

  server.send(200, "text/html", html);
}

// =====================================================
// Add/Edit Formular
// =====================================================

static void handleAddEditForm(bool editMode) {
  int index = -1;

  if (editMode) {
    if (!server.hasArg("i")) {
      server.send(400, "text/plain", "Missing index");
      return;
    }

    index = server.arg("i").toInt();

    if (!vaultIsValidIndex(index)) {
      server.send(404, "text/plain", "Entry not found");
      return;
    }
  }

  char generated[33];
  generateStrongPassword(generated, sizeof(generated), 20);

  String html = pageHeader(editMode ? "Edit Entry" : "Add Entry");

  html += "<div class='card'>";
  html += "<form method='POST' action='";
  html += editMode ? "/save_edit" : "/save_add";
  html += "'>";

  if (editMode) {
    html += "<input type='hidden' name='i' value='" + String(index) + "'>";
  }

  html += "<label>Title</label>";
  html += "<input name='title' maxlength='31' value='";
  html += editMode ? htmlEscape(vaultGetTitle(index)) : "";
  html += "' placeholder='Example: Gmail'>";

  html += "<label>Username / Email</label>";
  html += "<input name='username' maxlength='63' value='";
  html += editMode ? htmlEscape(vaultGetUsername(index)) : "";
  html += "' placeholder='name@example.com'>";

  html += "<label>Password</label>";
  html += "<input name='password' maxlength='95' value='";
  html += editMode ? "" : htmlEscape(generated);
  html += "' placeholder='Leave empty to keep old password'>";

  html += "<p class='muted'>";
  if (editMode) {
    html += "When editing: leave password empty to keep the old password.";
  } else {
    html += "A strong password was generated automatically. You can edit it.";
  }
  html += "</p>";

  html += "<label>Category</label>";
  html += "<input name='category' maxlength='23' value='";
  html += editMode ? htmlEscape(vaultGetCategory(index)) : "General";
  html += "' placeholder='Email / Finance / Dev'>";

  html += "<div class='actions'>";
  html += "<button class='btn' type='submit'>Save</button>";
  html += "<a class='btn secondary' href='/'>Cancel</a>";
  html += "</div>";

  html += "</form></div>";
  html += pageFooter();

  server.send(200, "text/html", html);
}

static void handleAdd() {
  webPortalMarkActivity();

  if (!requirePortalLogin()) {
    return;
  }

  handleAddEditForm(false);
}

static void handleEdit() {
  webPortalMarkActivity();

  if (!requirePortalLogin()) {
    return;
  }

  handleAddEditForm(true);
}

// =====================================================
// Save Add
// =====================================================

static void handleSaveAdd() {
  webPortalMarkActivity();

  if (!requirePortalLogin()) {
    return;
  }

  String title = server.arg("title");
  String username = server.arg("username");
  String password = server.arg("password");
  String category = server.arg("category");

  title.trim();
  username.trim();
  password.trim();
  category.trim();

  if (category.length() == 0) {
    category = "General";
  }

  if (title.length() == 0 || password.length() == 0) {
    String html = pageHeader("Error");
    html += "<div class='card'><h2>Title and password are required.</h2>";
    html += urlBack();
    html += "</div>" + pageFooter();
    server.send(200, "text/html", html);
    return;
  }

  bool ok = vaultAddEntry(title.c_str(), username.c_str(), password.c_str(), category.c_str());

  if (!ok) {
    String html = pageHeader("Error");
    html += "<div class='card'><h2>Could not add entry.</h2><p>Storage may be full.</p>";
    html += urlBack();
    html += "</div>" + pageFooter();
    server.send(200, "text/html", html);
    return;
  }

  server.sendHeader("Location", "/");
  server.send(303);
}

// =====================================================
// Save Edit
// =====================================================

static void handleSaveEdit() {
  webPortalMarkActivity();

  if (!requirePortalLogin()) {
    return;
  }

  if (!server.hasArg("i")) {
    server.send(400, "text/plain", "Missing index");
    return;
  }

  int index = server.arg("i").toInt();

  String title = server.arg("title");
  String username = server.arg("username");
  String password = server.arg("password");
  String category = server.arg("category");

  title.trim();
  username.trim();
  password.trim();
  category.trim();

  if (category.length() == 0) {
    category = "General";
  }

  bool ok =
      vaultUpdateEntry(index, title.c_str(), username.c_str(), password.c_str(), category.c_str());

  if (!ok) {
    String html = pageHeader("Error");
    html += "<div class='card'><h2>Could not update entry.</h2>";
    html += urlBack();
    html += "</div>" + pageFooter();
    server.send(200, "text/html", html);
    return;
  }

  server.sendHeader("Location", "/");
  server.send(303);
}

// =====================================================
// Delete
// =====================================================

static void handleDelete() {
  webPortalMarkActivity();

  if (!requirePortalLogin()) {
    return;
  }

  if (!server.hasArg("i")) {
    server.send(400, "text/plain", "Missing index");
    return;
  }

  int index = server.arg("i").toInt();

  String html = pageHeader("Delete Entry");

  html += "<div class='card'>";
  html += "<h2>Delete ";
  html += htmlEscape(vaultGetTitle(index));
  html += "?</h2>";
  html += "<p class='muted'>This cannot be undone.</p>";
  html += "<div class='actions'>";
  html += "<a class='btn danger' href='/delete_confirm?i=" + String(index) + "'>Delete</a>";
  html += "<a class='btn secondary' href='/'>Cancel</a>";
  html += "</div>";
  html += "</div>" + pageFooter();

  server.send(200, "text/html", html);
}

static void handleDeleteConfirm() {
  webPortalMarkActivity();

  if (!requirePortalLogin()) {
    return;
  }

  if (!server.hasArg("i")) {
    server.send(400, "text/plain", "Missing index");
    return;
  }

  int index = server.arg("i").toInt();
  vaultDeleteEntry(index);

  server.sendHeader("Location", "/");
  server.send(303);
}

// =====================================================
// Wi-Fi Settings Form
// =====================================================

static void handleWifiSettings() {
  webPortalMarkActivity();

  if (!requirePortalLogin()) {
    return;
  }

  String html = pageHeader("Wi-Fi Settings");

  html += "<div class='card'>";
  html += "<h2>TOTP Time Sync Wi-Fi</h2>";
  html += "<p class='muted'>These credentials are used only for NTP time sync for TOTP.</p>";

  html += "<form method='POST' action='/wifi_save'>";

  html += "<label>Wi-Fi SSID</label>";
  html += "<input name='ssid' maxlength='63' value='";
  html += htmlEscape(wifiSettingsGetSsid());
  html += "' placeholder='Your Wi-Fi name'>";

  html += "<label>Wi-Fi Password</label>";

  if (wifiSettingsHasCredentials()) {
    html +=
        "<input name='password' maxlength='95' type='password' value='' placeholder='Leave empty "
        "to keep current password'>";
    html +=
        "<p class='muted'>A Wi-Fi password is already saved. Leave this field empty to keep "
        "it.</p>";
  } else {
    html +=
        "<input name='password' maxlength='95' type='password' value='' placeholder='Your Wi-Fi "
        "password'>";
  }

  html += "<div class='actions'>";
  html += "<button class='btn' type='submit'>Save Wi-Fi</button>";
  html += "<a class='btn secondary' href='/'>Cancel</a>";
  html += "</div>";

  html += "</form>";
  html += "</div>";

  html += "<div class='card'>";
  html += "<h2>Status</h2>";

  if (wifiSettingsHasCredentials()) {
    html += "<p>Wi-Fi credentials are saved.</p>";
  } else {
    html += "<p class='muted'>No Wi-Fi credentials saved yet.</p>";
  }

  html += "</div>";

  html += pageFooter();

  server.send(200, "text/html", html);
}

// =====================================================
// Wi-Fi Settings speichern
// =====================================================

static void handleWifiSave() {
  webPortalMarkActivity();

  if (!requirePortalLogin()) {
    return;
  }

  String ssid = server.arg("ssid");
  String password = server.arg("password");

  ssid.trim();
  password.trim();

  if (ssid.length() == 0) {
    String html = pageHeader("Wi-Fi Error");
    html += "<div class='card'>";
    html += "<h2>SSID is required.</h2>";
    html += urlBack();
    html += "</div>";
    html += pageFooter();

    server.send(200, "text/html", html);
    return;
  }

  if (password.length() == 0 && !wifiSettingsHasCredentials()) {
    String html = pageHeader("Wi-Fi Error");
    html += "<div class='card'>";
    html += "<h2>Wi-Fi password is required.</h2>";
    html += urlBack();
    html += "</div>";
    html += pageFooter();

    server.send(200, "text/html", html);
    return;
  }

  if (password.length() == 0 && wifiSettingsHasCredentials()) {
    wifiSettingsSetRam(ssid.c_str(), wifiSettingsGetPassword());
  } else {
    wifiSettingsSetRam(ssid.c_str(), password.c_str());
  }

  bool secretsOk = secretsSecureStorageSave();
  wifiSettingsClearPlaintextFlash();

  DEBUG_PRINT("Encrypted secrets save after Wi-Fi: ");
  DEBUG_PRINTLN(secretsOk ? "OK" : "FAILED");

  String html = pageHeader("Wi-Fi Saved");
  html += "<div class='card'>";
  html += "<h2>Wi-Fi settings saved.</h2>";
  html += "<p class='muted'>Now TOTP time sync can use this Wi-Fi.</p>";
  html += "<p><a class='btn' href='/'>Back to Vault</a></p>";
  html += "</div>";
  html += pageFooter();

  server.send(200, "text/html", html);
}

// =====================================================
// TOTP Settings Form
// =====================================================

static void handleTotpSettings() {
  webPortalMarkActivity();

  if (!requirePortalLogin()) {
    return;
  }

  int editIndex = -1;

  if (server.hasArg("edit")) {
    editIndex = server.arg("edit").toInt();

    if (editIndex < 0 || editIndex >= totpSettingsGetCount()) {
      editIndex = -1;
    }
  }

  bool isEditMode = editIndex >= 0;

  String html = pageHeader("TOTP Settings");

  html += "<div class='card'>";
  html += "<h2>";
  html += isEditMode ? "Edit TOTP Account" : "Add TOTP Account";
  html += "</h2>";
  html += "<p class='muted'>TOTP secrets are stored encrypted in /secrets.enc.</p>";

  html += "<form method='POST' action='";
  html += isEditMode ? "/totp_update" : "/totp_save";
  html += "'>";

  if (isEditMode) {
    html += "<input type='hidden' name='i' value='";
    html += String(editIndex);
    html += "'>";
  }

  html += "<label>Label</label>";
  html += "<input name='label' maxlength='31' value='";

  if (isEditMode) {
    html += htmlEscape(totpSettingsGetLabelAt(editIndex));
  }

  html += "' placeholder='Example: Gmail'>";

  html += "<label>Base32 Secret</label>";

  if (isEditMode) {
    html +=
        "<input name='secret' maxlength='127' type='password' value='' placeholder='Leave empty to "
        "keep current secret'>";
    html +=
        "<p class='muted'>A secret is already saved for this account. Leave empty to keep it.</p>";
  } else {
    html +=
        "<input name='secret' maxlength='127' type='password' value='' placeholder='ABCD EFGH IJKL "
        "MNOP'>";
    html += "<p class='muted'>Enter the Base32 secret from your 2FA setup. Spaces are okay.</p>";
  }

  html += "<div class='actions'>";
  html += "<button class='btn' type='submit'>";
  html += isEditMode ? "Save Changes" : "Add TOTP";
  html += "</button>";
  html += "<a class='btn secondary' href='/totp'>Cancel</a>";
  html += "</div>";

  html += "</form>";
  html += "</div>";

  html += "<div class='card'>";
  html += "<h2>Saved TOTP Accounts</h2>";

  int count = totpSettingsGetCount();

  if (count == 0) {
    html += "<p class='muted'>No TOTP accounts saved yet.</p>";
  } else {
    html += "<table><tr><th>Label</th><th>Status</th><th>Actions</th></tr>";

    for (int i = 0; i < count; i++) {
      html += "<tr>";
      html += "<td>";
      html += htmlEscape(totpSettingsGetLabelAt(i));
      html += "</td>";
      html += "<td>Secret saved</td>";
      html += "<td>";
      html += "<a class='btn secondary' href='/totp?edit=";
      html += String(i);
      html += "'>Edit</a> ";
      html += "<a class='btn danger' href='/totp_delete?i=";
      html += String(i);
      html += "'>Delete</a>";
      html += "</td>";
      html += "</tr>";
    }

    html += "</table>";
  }

  html += "</div>";

  html += pageFooter();

  server.send(200, "text/html", html);
}

// =====================================================
// TOTP Settings speichern
// =====================================================

static void handleTotpSave() {
  webPortalMarkActivity();

  if (!requirePortalLogin()) {
    return;
  }

  String label = server.arg("label");
  String secret = server.arg("secret");

  label.trim();
  secret.trim();

  if (label.length() == 0) {
    label = "TOTP";
  }

  if (secret.length() == 0) {
    String html = pageHeader("TOTP Error");
    html += "<div class='card'>";
    html += "<h2>Base32 secret is required.</h2>";
    html += urlBack();
    html += "</div>";
    html += pageFooter();

    server.send(200, "text/html", html);
    return;
  }

  bool added = totpSettingsAddAccount(label.c_str(), secret.c_str());

  if (!added) {
    String html = pageHeader("TOTP Error");
    html += "<div class='card'>";
    html += "<h2>Could not add TOTP account.</h2>";
    html += "<p class='muted'>Storage may be full or secret is empty.</p>";
    html += urlBack();
    html += "</div>";
    html += pageFooter();

    server.send(200, "text/html", html);
    return;
  }

  bool secretsOk = secretsSecureStorageSave();
  totpSettingsClearPlaintextFlash();

  DEBUG_PRINT("Encrypted secrets save after TOTP add: ");
  DEBUG_PRINTLN(secretsOk ? "OK" : "FAILED");

  totpBegin();

  server.sendHeader("Location", "/totp");
  server.send(303);
}

static void handleTotpUpdate() {
  webPortalMarkActivity();

  if (!requirePortalLogin()) {
    return;
  }

  if (!server.hasArg("i")) {
    server.send(400, "text/plain", "Missing index");
    return;
  }

  int index = server.arg("i").toInt();

  String label = server.arg("label");
  String secret = server.arg("secret");

  label.trim();
  secret.trim();

  if (label.length() == 0) {
    label = "TOTP";
  }

  bool updated = totpSettingsUpdateAccount(index, label.c_str(), secret.c_str());

  if (!updated) {
    String html = pageHeader("TOTP Error");
    html += "<div class='card'>";
    html += "<h2>Could not update TOTP account.</h2>";
    html += urlBack();
    html += "</div>";
    html += pageFooter();

    server.send(200, "text/html", html);
    return;
  }

  bool secretsOk = secretsSecureStorageSave();
  totpSettingsClearPlaintextFlash();

  DEBUG_PRINT("Encrypted secrets save after TOTP update: ");
  DEBUG_PRINTLN(secretsOk ? "OK" : "FAILED");

  totpBegin();

  server.sendHeader("Location", "/totp");
  server.send(303);
}

static void handleTotpDelete() {
  webPortalMarkActivity();

  if (!requirePortalLogin()) {
    return;
  }

  if (!server.hasArg("i")) {
    server.send(400, "text/plain", "Missing index");
    return;
  }

  int index = server.arg("i").toInt();

  bool deleted = totpSettingsDeleteAccount(index);

  if (!deleted) {
    String html = pageHeader("TOTP Error");
    html += "<div class='card'>";
    html += "<h2>Could not delete TOTP account.</h2>";
    html += urlBack();
    html += "</div>";
    html += pageFooter();

    server.send(200, "text/html", html);
    return;
  }

  bool secretsOk = secretsSecureStorageSave();
  totpSettingsClearPlaintextFlash();

  DEBUG_PRINT("Encrypted secrets save after TOTP delete: ");
  DEBUG_PRINTLN(secretsOk ? "OK" : "FAILED");

  totpBegin();

  server.sendHeader("Location", "/totp");
  server.send(303);
}

// =====================================================
// TOTP Settings löschen
// =====================================================

static void handleTotpClear() {
  webPortalMarkActivity();

  if (!requirePortalLogin()) {
    return;
  }

  totpSettingsClearRam();
  totpBegin();

  bool secretsOk = secretsSecureStorageSave();
  totpSettingsClearPlaintextFlash();

  DEBUG_PRINT("Encrypted secrets save after TOTP clear: ");
  DEBUG_PRINTLN(secretsOk ? "OK" : "FAILED");

  String html = pageHeader("TOTP Cleared");
  html += "<div class='card'>";
  html += "<h2>TOTP settings cleared.</h2>";
  html += "<p><a class='btn' href='/totp'>Back to TOTP</a></p>";
  html += "</div>";
  html += pageFooter();

  server.send(200, "text/html", html);
}

// =====================================================
// Backup Export
// Zeigt verschlüsselte Dateien als Text.
// Wichtig: Das Backup bleibt verschlüsselt.
// =====================================================

static void handleBackupExport() {
  webPortalMarkActivity();

  if (!requirePortalLogin()) {
    return;
  }

  String vaultData = "";
  String secretsData = "";

  bool vaultOk = vaultFileReadText("/vault.enc", vaultData);
  bool secretsOk = vaultFileReadText("/secrets.enc", secretsData);

  String html = pageHeader("Backup Export");

  html += "<div class='card'>";
  html += "<h2>Encrypted Backup</h2>";
  html +=
      "<p class='muted'>Copy this backup and store it safely. It is encrypted with your current "
      "PIN-derived key.</p>";
  html +=
      "<p class='muted'>To restore it later, you need the same PIN or a compatible restore "
      "flow.</p>";
  html += "</div>";

  html += "<div class='card'>";
  html += "<h2>Status</h2>";

  html += "<p><strong>vault.enc:</strong> ";
  html += vaultOk ? "OK" : "MISSING";
  html += "</p>";

  html += "<p><strong>secrets.enc:</strong> ";
  html += secretsOk ? "OK" : "MISSING";
  html += "</p>";

  html += "</div>";

  html += "<div class='card'>";
  html += "<h2>Backup Text</h2>";
  html += "<p class='muted'>Select all text below and copy it.</p>";

  html +=
      "<textarea readonly "
      "style='width:100%;height:420px;box-sizing:border-box;border-radius:12px;background:#0e0e0e;"
      "color:#fff;border:1px solid #333;padding:12px;font-family:monospace;font-size:12px;'>";

  html += "-----BEGIN SECUREVAULT BACKUP V1-----\n";

  html += "-----BEGIN VAULT.ENC-----\n";
  if (vaultOk) {
    html += vaultData;
  }
  html += "-----END VAULT.ENC-----\n";

  html += "-----BEGIN SECRETS.ENC-----\n";
  if (secretsOk) {
    html += secretsData;
  }
  html += "-----END SECRETS.ENC-----\n";

  html += "-----END SECUREVAULT BACKUP V1-----\n";

  html += "</textarea>";

  html += "<p><a class='btn secondary' href='/info'>Back</a></p>";
  html += "</div>";

  html += pageFooter();

  server.send(200, "text/html", html);
}

// =====================================================
// Backup Import Helfer
// =====================================================

static bool extractBackupSection(const String& backup, const char* beginMarker,
                                 const char* endMarker, String& output) {
  output = "";

  int beginIndex = backup.indexOf(beginMarker);
  int endIndex = backup.indexOf(endMarker);

  if (beginIndex < 0 || endIndex < 0 || endIndex <= beginIndex) {
    return false;
  }

  beginIndex += strlen(beginMarker);

  if (beginIndex < backup.length() && backup[beginIndex] == '\r') {
    beginIndex++;
  }

  if (beginIndex < backup.length() && backup[beginIndex] == '\n') {
    beginIndex++;
  }

  output = backup.substring(beginIndex, endIndex);

  while (output.endsWith("\n") || output.endsWith("\r")) {
    output.remove(output.length() - 1);
  }

  return output.length() > 0;
}

// =====================================================
// Backup Import Formular
// =====================================================

static void handleBackupImportForm() {
  webPortalMarkActivity();

  if (!requirePortalLogin()) {
    return;
  }

  String html = pageHeader("Backup Import");

  html += "<div class='card'>";
  html += "<h2>Import Encrypted Backup</h2>";
  html +=
      "<p class='muted'>Paste a SecureVault backup below. This overwrites the current encrypted "
      "vault and secrets files.</p>";
  html += "<p class='muted'>Only import backups you created yourself.</p>";
  html += "</div>";

  html += "<div class='card'>";
  html += "<form method='POST' action='/backup_import_submit'>";

  html += "<label>Backup Text</label>";
  html +=
      "<textarea name='backup' "
      "style='width:100%;height:360px;box-sizing:border-box;border-radius:12px;background:#0e0e0e;"
      "color:#fff;border:1px solid "
      "#333;padding:12px;font-family:monospace;font-size:12px;'></textarea>";

  html += "<label>Type IMPORT to confirm</label>";
  html += "<input name='confirm' maxlength='10' placeholder='IMPORT'>";

  html += "<div class='actions'>";
  html += "<button class='btn danger' type='submit'>Import Backup</button>";
  html += "<a class='btn secondary' href='/info'>Cancel</a>";
  html += "</div>";

  html += "</form>";
  html += "</div>";

  html += pageFooter();

  server.send(200, "text/html", html);
}

// =====================================================
// Backup Import ausführen
// =====================================================

static void handleBackupImportSubmit() {
  webPortalMarkActivity();

  if (!requirePortalLogin()) {
    return;
  }

  String confirm = server.arg("confirm");
  confirm.trim();

  if (confirm != "IMPORT") {
    String html = pageHeader("Import Cancelled");
    html += "<div class='card'>";
    html += "<h2>Import was not confirmed.</h2>";
    html += "<p class='muted'>You must type IMPORT exactly.</p>";
    html += "<p><a class='btn secondary' href='/backup_import'>Back</a></p>";
    html += "</div>";
    html += pageFooter();

    server.send(200, "text/html", html);
    return;
  }

  String backup = server.arg("backup");

  String vaultData = "";
  String secretsData = "";

  bool vaultFound = extractBackupSection(backup, "-----BEGIN VAULT.ENC-----",
                                         "-----END VAULT.ENC-----", vaultData);

  bool secretsFound = extractBackupSection(backup, "-----BEGIN SECRETS.ENC-----",
                                           "-----END SECRETS.ENC-----", secretsData);

  if (!vaultFound || !secretsFound) {
    String html = pageHeader("Import Error");
    html += "<div class='card'>";
    html += "<h2>Backup format is invalid.</h2>";
    html += "<p class='muted'>Missing vault.enc or secrets.enc section.</p>";
    html += "<p><a class='btn secondary' href='/backup_import'>Back</a></p>";
    html += "</div>";
    html += pageFooter();

    server.send(200, "text/html", html);
    return;
  }

  // Alte Dateien merken, damit wir bei Fehler rollback machen können.
  String oldVaultData = "";
  String oldSecretsData = "";

  bool oldVaultOk = vaultFileReadText("/vault.enc", oldVaultData);
  bool oldSecretsOk = vaultFileReadText("/secrets.enc", oldSecretsData);

  bool vaultWriteOk = vaultFileWriteText("/vault.enc", vaultData);
  bool secretsWriteOk = vaultFileWriteText("/secrets.enc", secretsData);

  bool vaultLoadOk = false;
  bool secretsLoadOk = false;
  bool rollbackDone = false;

  if (vaultWriteOk && secretsWriteOk) {
    vaultLoadOk = vaultSecureStorageLoad();
    secretsLoadOk = secretsSecureStorageLoad();
    totpBegin();
  }

  bool importOk = vaultWriteOk && secretsWriteOk && vaultLoadOk && secretsLoadOk;

  if (!importOk) {
    // Rollback: alte funktionierende Dateien zurückschreiben.
    if (oldVaultOk) {
      vaultFileWriteText("/vault.enc", oldVaultData);
    }

    if (oldSecretsOk) {
      vaultFileWriteText("/secrets.enc", oldSecretsData);
    }

    if (oldVaultOk || oldSecretsOk) {
      vaultSecureStorageLoad();
      secretsSecureStorageLoad();
      totpBegin();
      rollbackDone = true;
    }
  }

  String html = pageHeader(importOk ? "Backup Imported" : "Import Failed");

  html += "<div class='card'>";
  html += "<h2>Import Result</h2>";

  html += "<p><strong>Write vault.enc:</strong> ";
  html += vaultWriteOk ? "OK" : "FAILED";
  html += "</p>";

  html += "<p><strong>Write secrets.enc:</strong> ";
  html += secretsWriteOk ? "OK" : "FAILED";
  html += "</p>";

  html += "<p><strong>Load vault:</strong> ";
  html += vaultLoadOk ? "OK" : "FAILED";
  html += "</p>";

  html += "<p><strong>Load secrets:</strong> ";
  html += secretsLoadOk ? "OK" : "FAILED";
  html += "</p>";

  html += "<p><strong>Rollback:</strong> ";
  html += rollbackDone ? "DONE" : "NOT NEEDED";
  html += "</p>";

  if (importOk) {
    html += "<p class='muted'>Backup imported and loaded successfully.</p>";
  } else {
    html +=
        "<p class='muted'>Backup could not be loaded with the current PIN/key. Old data was "
        "restored if available.</p>";
  }

  html += "<p><a class='btn' href='/'>Back to Vault</a></p>";
  html += "</div>";

  html += pageFooter();

  server.send(200, "text/html", html);
}

// =====================================================
// Info
// =====================================================

static void handleInfo() {
  webPortalMarkActivity();

  if (!requirePortalLogin()) {
    return;
  }

  String html = pageHeader("Info");

  html += "<div class='card'>";
  html += "<h2>SecureVault Local Web Portal</h2>";
  html += "<p>This web portal runs directly on the ESP32 device.</p>";
  html += "<p>No cloud. No external account. No remote server.</p>";
  html += "<p class='muted'>The portal automatically stops after 10 minutes without activity.</p>";
  html += "</div>";

  html += "<div class='card'>";
  html += "<h2>Current Portal Wi-Fi</h2>";
  html += "<p><strong>SSID:</strong> ";
  html += htmlEscape(webPortalGetSsid());
  html += "</p>";
  html +=
      "<p class='muted'>The Wi-Fi password is shown only on the device display while the portal is "
      "running.</p>";
  html +=
      "<p class='muted'>If your phone or PC remembered an old password, use Forget Network "
      "once.</p>";
  html += "</div>";

  html += "<div class='card'>";
  html += "<h2>Security Status</h2>";
  html += "<p class='muted'>Vault entries are stored encrypted in /vault.enc.</p>";
  html +=
      "<p class='muted'>Wi-Fi credentials and TOTP secrets are stored encrypted in "
      "/secrets.enc.</p>";
  html +=
      "<p class='muted'>Master key, vault RAM, Wi-Fi secrets and TOTP secrets are cleared when the "
      "device locks.</p>";
  html += "</div>";

  html += "<div class='card'>";
  html += "<h2>Backup</h2>";
  html += "<p class='muted'>Export encrypted vault and secrets backup.</p>";
  html += "<div class='actions'>";
  html += "<a class='btn' href='/backup_export'>Export Backup</a>";
  html += "<a class='btn danger' href='/backup_import'>Import Backup</a>";
  html += "</div>";
  html += "</div>";

  html += pageFooter();

  server.send(200, "text/html", html);
}

// =====================================================
// Browser-Nebenanfragen ignorieren
// Beispiele: favicon, manifests, browser icons
// =====================================================

static void handleEmptyResponse() {
  webPortalMarkActivity();
  server.send(204, "text/plain", "");
}

static void handleNotFoundQuietly() {
  webPortalMarkActivity();

  String uri = server.uri();

  if (uri == "/favicon.ico" || uri == "/robots.txt" || uri == "/manifest.json" ||
      uri == "/site.webmanifest" || uri == "/browserconfig.xml" || uri == "/apple-touch-icon.png" ||
      uri == "/apple-touch-icon-precomposed.png" || uri == "/apple-touch-icon-120x120.png" ||
      uri == "/apple-touch-icon-152x152.png" || uri == "/apple-touch-icon-180x180.png" ||
      uri == "/android-chrome-192x192.png" || uri == "/android-chrome-512x512.png" ||
      uri == "/generate_204" || uri == "/hotspot-detect.html" || uri == "/ncsi.txt" ||
      uri == "/connecttest.txt" || uri == "/fwlink" || uri == "/success.txt") {
    server.send(204, "text/plain", "");
    return;
  }

  server.send(404, "text/plain", "Not found");
}

// =====================================================
// Portal starten / stoppen
// =====================================================

bool webPortalStart() {
  if (portalActive) {
    return true;
  }

  portalPassword = loadOrCreatePortalPassword();
  portalAuthenticated = false;
  portalFailedPinAttempts = 0;
  portalLoginLocked = false;
  portalLoginLockedAt = 0;

  WiFi.disconnect(true, true);
  WiFi.softAPdisconnect(true);
  delay(200);

  WiFi.mode(WIFI_AP);
  delay(100);

  bool ok = WiFi.softAP(PORTAL_SSID, portalPassword.c_str());

  if (!ok) {
    WiFi.mode(WIFI_OFF);
    return false;
  }

  portalIp = WiFi.softAPIP().toString();

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  server.on("/login", HTTP_GET, handleLogin);
  server.on("/login_submit", HTTP_POST, handleLoginSubmit);
  server.on("/logout", HTTP_GET, handleLogout);

  server.on("/favicon.ico", HTTP_ANY, handleEmptyResponse);
  server.on("/robots.txt", HTTP_ANY, handleEmptyResponse);
  server.on("/manifest.json", HTTP_ANY, handleEmptyResponse);
  server.on("/site.webmanifest", HTTP_ANY, handleEmptyResponse);
  server.on("/browserconfig.xml", HTTP_ANY, handleEmptyResponse);
  server.on("/apple-touch-icon.png", HTTP_ANY, handleEmptyResponse);
  server.on("/apple-touch-icon-precomposed.png", HTTP_ANY, handleEmptyResponse);
  server.on("/apple-touch-icon-120x120.png", HTTP_ANY, handleEmptyResponse);
  server.on("/apple-touch-icon-152x152.png", HTTP_ANY, handleEmptyResponse);
  server.on("/apple-touch-icon-180x180.png", HTTP_ANY, handleEmptyResponse);
  server.on("/android-chrome-192x192.png", HTTP_ANY, handleEmptyResponse);
  server.on("/android-chrome-512x512.png", HTTP_ANY, handleEmptyResponse);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/add", HTTP_GET, handleAdd);
  server.on("/edit", HTTP_GET, handleEdit);
  server.on("/wifi", HTTP_GET, handleWifiSettings);
  server.on("/wifi_save", HTTP_POST, handleWifiSave);
  server.on("/totp", HTTP_GET, handleTotpSettings);
  server.on("/totp_save", HTTP_POST, handleTotpSave);
  server.on("/totp_update", HTTP_POST, handleTotpUpdate);
  server.on("/totp_delete", HTTP_GET, handleTotpDelete);
  server.on("/totp_clear", HTTP_GET, handleTotpClear);
  server.on("/save_add", HTTP_POST, handleSaveAdd);
  server.on("/save_edit", HTTP_POST, handleSaveEdit);
  server.on("/delete", HTTP_GET, handleDelete);
  server.on("/delete_confirm", HTTP_GET, handleDeleteConfirm);
  server.on("/info", HTTP_GET, handleInfo);
  server.on("/backup_export", HTTP_GET, handleBackupExport);
  server.on("/backup_import", HTTP_GET, handleBackupImportForm);
  server.on("/backup_import_submit", HTTP_POST, handleBackupImportSubmit);
  server.on("/generate_204", HTTP_ANY, handleEmptyResponse);
  server.on("/hotspot-detect.html", HTTP_ANY, handleEmptyResponse);
  server.on("/ncsi.txt", HTTP_ANY, handleEmptyResponse);
  server.on("/connecttest.txt", HTTP_ANY, handleEmptyResponse);
  server.on("/fwlink", HTTP_ANY, handleEmptyResponse);
  server.on("/success.txt", HTTP_ANY, handleEmptyResponse);

  server.onNotFound(handleNotFoundQuietly);
  server.begin();

  portalActive = true;
  webPortalMarkActivity();
  return true;
}

void webPortalStop() {
  if (!portalActive) {
    return;
  }

  dnsServer.stop();
  server.stop();

  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);

  portalActive = false;
  portalAuthenticated = false;
  portalFailedPinAttempts = 0;
  portalLoginLocked = false;
  portalLoginLockedAt = 0;
}

void webPortalProcess() {
  if (!portalActive) {
    return;
  }

  dnsServer.processNextRequest();
  server.handleClient();
}

bool webPortalIsActive() {
  return portalActive;
}

const char* webPortalGetSsid() {
  return PORTAL_SSID;
}

const char* webPortalGetPassword() {
  return portalPassword.c_str();
}

const char* webPortalGetIp() {
  return portalIp.c_str();
}