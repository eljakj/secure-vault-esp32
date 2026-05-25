#pragma once

#include <Arduino.h>
#include "config.h"

// =====================================================
// Debug Logging
// Im DEV_MODE aktiv, im PRODUCT_MODE still.
// =====================================================

#if SECUREVAULT_DEV_MODE
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif