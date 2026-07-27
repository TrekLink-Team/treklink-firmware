/*
 * TrekLink v4.0 LilyGo T-Beam Supreme Overlay Variant
 * Includes stock T-Beam Supreme (tbeam-s3-core) pinouts and overrides TrekLink-specific configurations.
 */

#pragma once

// 1. Import stock T-Beam Supreme pin definitions
#include "../tbeam-s3-core/variant.h"

// 2. TrekLink overlay definitions
#ifndef TREKLINK_VARIANT
#define TREKLINK_VARIANT
#endif

#ifndef TREKLINK_V4
#define TREKLINK_V4
#endif

// 3. Regional and timezone defaults
#undef REGULATORY_LORA_REGIONCODE
#define REGULATORY_LORA_REGIONCODE meshtastic_Config_LoRaConfig_RegionCode_MY_433

#undef DEFAULT_TIMEZONE
#define DEFAULT_TIMEZONE "ICT-7"

// 4. Default canned messages list
#undef CANNED_MESSAGE_MODULE_MESSAGES_DEFAULT
#define CANNED_MESSAGE_MODULE_MESSAGES_DEFAULT                                                                                     \
    "MEDICAL-HELP|LOST-HELP|SAFE|EVAC|LOW BAT|COMING|FOUND|LOST|"                                                                  \
    "OK|NO|CHECK?|STOP|GO|WAIT ME|COME2ME|TURNBACK|MEET AT|REQST|"                                                                 \
    "N|S|E|W|R|L|TRAIL|CHKPT|CAMP|DANGR|LO|HI|SPPLY|FD|WTER"
