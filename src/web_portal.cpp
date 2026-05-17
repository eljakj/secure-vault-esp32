#include "web_portal.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>

#include "vault_model.h"
#include "password_generator.h"

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
  html += "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>SecureVault</title>";
  html += "<style>";
  html += "body{margin:0;background:#0b0b0b;color:#f5f5f5;font-family:Arial,sans-serif;padding:18px;}";
  html += ".wrap{max-width:980px;margin:0 auto;}";
  html += "h1{font-size:28px;margin:0 0 18px;text-align:center;}";
  html += "h2{font-size:20px;margin:0 0 14px;}";
  html += ".card{background:#161616;border:1px solid #2a2a2a;border-radius:18px;padding:18px;margin-bottom:16px;}";
  html += "label{display:block;margin:12px 0 6px;color:#d0d0d0;font-weight:700;}";
  html += "input,select{width:100%;box-sizing:border-box;padding:13px;border-radius:12px;border:1px solid #333;background:#0e0e0e;color:#fff;font-size:16px;}";
  html += "table{width:100%;border-collapse:collapse;}";
  html += "th,td{border-bottom:1px solid #2a2a2a;padding:12px 8px;text-align:left;vertical-align:middle;}";
  html += "th{color:#aaa;font-size:13px;text-transform:uppercase;}";
  html += ".actions{display:flex;gap:8px;flex-wrap:wrap;margin-top:14px;}";
  html += ".btn{display:inline-block;border:0;border-radius:12px;padding:11px 14px;background:#2979ff;color:#fff;text-decoration:none;font-weight:700;cursor:pointer;}";
  html += ".danger{background:#d32f2f;}";
  html += ".secondary{background:#444;}";
  html += ".muted{color:#aaa;font-size:14px;}";
  html += ".top{display:flex;gap:10px;flex-wrap:wrap;justify-content:center;margin-bottom:16px;}";
  html += "</style></head><body><div class='wrap'>";
  html += "<h1>" + title + "</h1>";
  html += "<div class='top'>";
  html += "<a class='btn' href='/'>Vault</a>";
  html += "<a class='btn' href='/add'>Add Entry</a>";
  html += "<a class='btn secondary' href='/info'>Info</a>";
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
  handleAddEditForm(false);
}

static void handleEdit() {
  handleAddEditForm(true);
}

// =====================================================
// Save Add
// =====================================================

static void handleSaveAdd() {
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

  bool ok = vaultUpdateEntry(index, title.c_str(), username.c_str(), password.c_str(), category.c_str());

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
// Info
// =====================================================

static void handleInfo() {
  String html = pageHeader("Info");

  html += "<div class='card'>";
  html += "<h2>SecureVault Local Web Portal</h2>";
  html += "<p>No cloud. This page is served directly from the ESP32.</p>";
  html += "<p class='muted'>Important: storage is not encrypted yet. Next step is encryption.</p>";
  html += "</div>" + pageFooter();

  server.send(200, "text/html", html);
}

// =====================================================
// Portal starten / stoppen
// =====================================================

bool webPortalStart() {
  if (portalActive) {
    return true;
  }

  portalPassword = loadOrCreatePortalPassword();

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

  server.on("/", HTTP_GET, handleRoot);
  server.on("/add", HTTP_GET, handleAdd);
  server.on("/edit", HTTP_GET, handleEdit);
  server.on("/save_add", HTTP_POST, handleSaveAdd);
  server.on("/save_edit", HTTP_POST, handleSaveEdit);
  server.on("/delete", HTTP_GET, handleDelete);
  server.on("/delete_confirm", HTTP_GET, handleDeleteConfirm);
  server.on("/info", HTTP_GET, handleInfo);

  // Captive portal helpers
  server.on("/generate_204", HTTP_GET, handleRoot);
  server.on("/hotspot-detect.html", HTTP_GET, handleRoot);
  server.on("/ncsi.txt", HTTP_GET, handleRoot);

  server.onNotFound(handleRoot);
  server.begin();

  portalActive = true;
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
