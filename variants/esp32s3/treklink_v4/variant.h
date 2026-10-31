/*
 * TrekLink v4, LilyGo T-Beam Supreme: the stock tbeam-s3-core pins, unchanged, plus TrekLink capabilities.
 * Parts and capabilities: specs/variants/v4/components.md. Overlay rules: specs/architecture.
 */
#pragma once

#include "../tbeam-s3-core/variant.h"

// Region VN 433, which is the regulatory MY_433 band (specs/architecture design §4.2 O-03)
#undef REGULATORY_LORA_REGIONCODE
#define REGULATORY_LORA_REGIONCODE meshtastic_Config_LoRaConfig_RegionCode_MY_433

// TrekLink capabilities (specs/architecture design §3.2). SOS uses the stock user button (BUTTON_PIN 0).
#define TREKLINK_VARIANT_NAME "TrekLink v4 (T-Beam Supreme)"
#define TREKLINK_FALL_SENSOR_QMI8658 1
