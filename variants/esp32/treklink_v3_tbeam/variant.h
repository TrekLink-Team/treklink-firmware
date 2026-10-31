/*
 * TrekLink v3.0 LilyGo T-Beam V1.2 Overlay Variant
 * Includes stock T-Beam V1.2 pinouts and overrides TrekLink-specific configurations.
 */

#pragma once

// 1. Import stock T-Beam V1.2 pin definitions
#include "../tbeam/variant.h"

// 2. TrekLink overlay definitions
#ifndef TREKLINK_VARIANT
#define TREKLINK_VARIANT
#endif

#ifndef TREKLINK_V3
#define TREKLINK_V3
#endif

// TrekLink: Meshtastic 2.7.26 (#9512) renamed the stock LED_PIN to LED_POWER. The TrekLink SOS strobe
// (TrekLinkSOSGesture.cpp, TrekLinkSOSHelper.cpp) still tests LED_PIN, so alias it as v1 and v2 do.
#if defined(LED_POWER) && !defined(LED_PIN)
#define LED_PIN LED_POWER
#endif

// 3. Regional and timezone defaults
#undef REGULATORY_LORA_REGIONCODE
#define REGULATORY_LORA_REGIONCODE meshtastic_Config_LoRaConfig_RegionCode_MY_433

#undef DEFAULT_TIMEZONE
#define DEFAULT_TIMEZONE "ICT-7"

// Default channel 0 position precision for TrekLink: 32 bits (full). Since Meshtastic 2.7.26 (#10383, #10513)
// Router::send clamps every position packet the node originates to its channel's precision, and the stock default
// of 13 bits (Channels.cpp initDefaultChannel) would coarsen the SOS position to about 0.05 degrees. Stock code
// reads this macro; a userPrefs.jsonc value still wins. Applies when the default channel is written (fresh
// install or factory reset), not to a channel already stored in flash.
#ifndef USERPREFS_CHANNEL_0_PRECISION
#define USERPREFS_CHANNEL_0_PRECISION 32
#endif

// 4. Default canned messages list
#undef CANNED_MESSAGE_MODULE_MESSAGES_DEFAULT
#define CANNED_MESSAGE_MODULE_MESSAGES_DEFAULT                                                                                     \
    "MEDICAL-HELP|LOST-HELP|SAFE|EVAC|LOW BAT|COMING|FOUND|LOST|"                                                                  \
    "OK|NO|CHECK?|STOP|GO|WAIT ME|COME2ME|TURNBACK|MEET AT|REQST|"                                                                 \
    "N|S|E|W|R|L|TRAIL|CHKPT|CAMP|DANGR|LO|HI|SPPLY|FD|WTER"
