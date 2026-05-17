#include "password_generator.h"

// =====================================================
// Starken zufälligen Passwort-String erzeugen
// =====================================================

void generateStrongPassword(char* output, size_t outputSize, int length) {
  const char charset[] =
    "ABCDEFGHJKLMNPQRSTUVWXYZ"
    "abcdefghijkmnopqrstuvwxyz"
    "23456789"
    "!@#$%^&*_-+=?";

  if (output == nullptr || outputSize == 0) {
    return;
  }

  if (length < 4) {
    length = 4;
  }

  if ((size_t)length >= outputSize) {
    length = outputSize - 1;
  }

  int charsetSize = sizeof(charset) - 1;

  for (int i = 0; i < length; i++) {
    output[i] = charset[esp_random() % charsetSize];
  }

  output[length] = '\0';
}
