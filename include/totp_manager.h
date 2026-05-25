#pragma once

#include <Arduino.h>

// =====================================================
// TOTP Manager
// =====================================================

bool totpBegin();
bool totpHasSecret();

bool totpSyncTimeWithWiFi(const char* ssid, const char* password);
bool totpIsTimeValid();

uint32_t totpGetCurrentCode();
uint32_t totpGenerateCodeFromBase32(const char* base32Secret);

int totpGetSecondsRemaining();

const char* totpGetLabel();