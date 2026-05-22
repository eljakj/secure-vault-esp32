#pragma once

#include <Arduino.h>

bool webPortalStart();
void webPortalStop();
void webPortalProcess();

bool webPortalIsActive();
const char* webPortalGetSsid();
const char* webPortalGetPassword();
const char* webPortalGetIp();

// =====================================================
// Web Portal PIN/Login
// =====================================================

void webPortalSetPinHash(const char* pinHash);
void webPortalLogout();

// =====================================================
// Web Portal Aktivität
// =====================================================

void webPortalMarkActivity();
unsigned long webPortalGetLastActivityTime();