/*
 * TrekLink v3, LilyGo T-Beam v1.2: the stock T-Beam pins, unchanged, plus TrekLink capabilities.
 * Parts and capabilities: specs/variants/v3/components.md. Overlay rules: specs/architecture.
 */
#pragma once

#include "../tbeam/variant.h"

// Region VN 433, which is the regulatory MY_433 band (specs/architecture design §4.2 O-03)
#undef REGULATORY_LORA_REGIONCODE
#define REGULATORY_LORA_REGIONCODE meshtastic_Config_LoRaConfig_RegionCode_MY_433

// TrekLink capabilities (specs/architecture design §3.2). SOS uses the stock user button (BUTTON_PIN 38).
#define TREKLINK_VARIANT_NAME "TrekLink v3 (T-Beam v1.2)"
#define TREKLINK_SOS_LED_PIN LED_POWER
