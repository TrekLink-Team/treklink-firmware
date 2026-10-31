/*
 * TrekLink capabilities (specs/architecture design §3.2).
 *
 * A variant's variant.h says what its board has; this header supplies the derived flags and stops
 * the build when a variant contradicts itself. Shared TrekLink code includes this header instead of
 * configuration.h and tests capabilities, never a variant name.
 */

#pragma once

#include "configuration.h"

#ifndef TREKLINK_VARIANT
#error "treklink/src is compiled only by TrekLink envs, which define TREKLINK_VARIANT (specs/architecture REQ-ARC-02)"
#endif

#ifndef TREKLINK_VARIANT_NAME
#error "The TrekLink variant.h must define TREKLINK_VARIANT_NAME"
#endif

// Overlay version, logged at boot next to the upstream version (REQ-ARC-08).
#ifndef TREKLINK_OVERLAY_VERSION
#define TREKLINK_OVERLAY_VERSION "overlay-1"
#endif

// SOS trigger: a dedicated button, or the gesture on the stock user button.
#if !defined(TREKLINK_SOS_BUTTON_PIN) && !defined(BUTTON_PIN)
#error "A TrekLink variant needs TREKLINK_SOS_BUTTON_PIN or a stock BUTTON_PIN for the SOS trigger"
#endif
#if defined(TREKLINK_SOS_BUTTON_PIN) && defined(BUTTON_PIN) && (TREKLINK_SOS_BUTTON_PIN == BUTTON_PIN)
#error "TREKLINK_SOS_BUTTON_PIN must not be the stock user button"
#endif

// Fall detection: at most one IMU adapter.
#if (defined(TREKLINK_FALL_SENSOR_ICM20948) + defined(TREKLINK_FALL_SENSOR_QMI8658)) > 1
#error "Define at most one TREKLINK_FALL_SENSOR_* in variant.h"
#endif
#if defined(TREKLINK_FALL_SENSOR_ICM20948) || defined(TREKLINK_FALL_SENSOR_QMI8658)
#if MESHTASTIC_EXCLUDE_I2C
#error "Fall detection needs I2C"
#endif
#define TREKLINK_HAS_FALL_DETECTION 1
#else
#define TREKLINK_HAS_FALL_DETECTION 0
#endif

#if defined(PIN_BUZZER) && !defined(BUZZER_LEDC_CHANNEL)
#define BUZZER_LEDC_CHANNEL 0
#endif
