#include "vault_serializer.h"

#include "vault_model.h"

// =====================================================
// Hilfsfunktion: Text escapen
// Wir benutzen ein einfaches Tab-getrenntes Format.
// \t, \n und \ werden escaped.
// =====================================================

static String escapeField(const char* input) {
  String output = "";

  if (input == nullptr) {
    return output;
  }

  for (int i = 0; input[i] != '\0'; i++) {
    char c = input[i];

    if (c == '\\') {
      output += "\\\\";
    } else if (c == '\t') {
      output += "\\t";
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

// =====================================================
// Hilfsfunktion: Text unescapen
// =====================================================

static String unescapeField(const String& input) {
  String output = "";

  for (int i = 0; i < input.length(); i++) {
    char c = input[i];

    if (c == '\\' && i + 1 < input.length()) {
      char next = input[i + 1];

      if (next == '\\') {
        output += '\\';
        i++;
      } else if (next == 't') {
        output += '\t';
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
// Sicheres Kopieren in char[]
// =====================================================

static void safeCopy(char* destination, size_t destinationSize, const String& source) {
  if (destination == nullptr || destinationSize == 0) {
    return;
  }

  source.toCharArray(destination, destinationSize);
  destination[destinationSize - 1] = '\0';
}

// =====================================================
// Eine Zeile parsen:
// title<TAB>username<TAB>password<TAB>category
// =====================================================

static bool parseLineToEntry(const String& line, VaultEntry& entry) {
  int p1 = line.indexOf('\t');
  if (p1 < 0)
    return false;

  int p2 = line.indexOf('\t', p1 + 1);
  if (p2 < 0)
    return false;

  int p3 = line.indexOf('\t', p2 + 1);
  if (p3 < 0)
    return false;

  String title = unescapeField(line.substring(0, p1));
  String username = unescapeField(line.substring(p1 + 1, p2));
  String password = unescapeField(line.substring(p2 + 1, p3));
  String category = unescapeField(line.substring(p3 + 1));

  if (title.length() == 0) {
    return false;
  }

  safeCopy(entry.title, sizeof(entry.title), title);
  safeCopy(entry.username, sizeof(entry.username), username);
  safeCopy(entry.password, sizeof(entry.password), password);
  safeCopy(entry.category, sizeof(entry.category), category);

  return true;
}

// =====================================================
// Vault zu Text serialisieren
// =====================================================

String vaultSerializeToText() {
  String output = "";

  output += "SECUREVAULT_V1\n";
  output += String(vaultCount);
  output += "\n";

  for (int i = 0; i < vaultCount; i++) {
    output += escapeField(vaultEntries[i].title);
    output += "\t";
    output += escapeField(vaultEntries[i].username);
    output += "\t";
    output += escapeField(vaultEntries[i].password);
    output += "\t";
    output += escapeField(vaultEntries[i].category);
    output += "\n";
  }

  return output;
}

// =====================================================
// Text zurück in Vault laden
// Achtung: Speichert noch NICHT automatisch in Flash.
// =====================================================

bool vaultDeserializeFromText(const String& data) {
  int firstLineEnd = data.indexOf('\n');
  if (firstLineEnd < 0)
    return false;

  String magic = data.substring(0, firstLineEnd);
  magic.trim();

  if (magic != "SECUREVAULT_V1") {
    return false;
  }

  int secondLineEnd = data.indexOf('\n', firstLineEnd + 1);
  if (secondLineEnd < 0)
    return false;

  String countText = data.substring(firstLineEnd + 1, secondLineEnd);
  countText.trim();

  int expectedCount = countText.toInt();

  if (expectedCount < 0 || expectedCount > MAX_VAULT_ENTRIES) {
    return false;
  }

  VaultEntry tempEntries[MAX_VAULT_ENTRIES];
  int parsedCount = 0;

  int lineStart = secondLineEnd + 1;

  while (lineStart < data.length() && parsedCount < expectedCount) {
    int lineEnd = data.indexOf('\n', lineStart);

    if (lineEnd < 0) {
      lineEnd = data.length();
    }

    String line = data.substring(lineStart, lineEnd);

    if (line.length() > 0) {
      VaultEntry entry;
      entry.title[0] = '\0';
      entry.username[0] = '\0';
      entry.password[0] = '\0';
      entry.category[0] = '\0';

      if (!parseLineToEntry(line, entry)) {
        return false;
      }

      tempEntries[parsedCount] = entry;
      parsedCount++;
    }

    lineStart = lineEnd + 1;
  }

  if (parsedCount != expectedCount) {
    return false;
  }

  vaultCount = parsedCount;

  for (int i = 0; i < MAX_VAULT_ENTRIES; i++) {
    if (i < vaultCount) {
      vaultEntries[i] = tempEntries[i];
    } else {
      vaultEntries[i].title[0] = '\0';
      vaultEntries[i].username[0] = '\0';
      vaultEntries[i].password[0] = '\0';
      vaultEntries[i].category[0] = '\0';
    }
  }

  return true;
}

// =====================================================
// Debug-Ausgabe ohne Passwort-Inhalt
// =====================================================

void vaultSerializerDebugPrint() {
  Serial.println();
  Serial.println("=== Vault Serializer Debug ===");

  String text = vaultSerializeToText();

  Serial.print("Vault entries: ");
  Serial.println(vaultCount);

  Serial.print("Serialized bytes: ");
  Serial.println(text.length());

  Serial.println("==============================");
}