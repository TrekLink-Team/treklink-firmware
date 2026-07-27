/*
 * TrekLinkVariantValidation.h — Compile-time variant validation
 *
 * Enforces that exactly one TrekLink version flag is set,
 * and that required GPIO defines exist for each variant's hardware configuration.
 *
 * Include this ONLY when TREKLINK_VARIANT is defined (guarded below).
 */

#pragma once

#ifdef TREKLINK_VARIANT

// ========================================================================
// 1. Mutual exclusivity: at most one version flag may be defined
// ========================================================================

#if (defined(TREKLINK_V2) + defined(TREKLINK_V3) + defined(TREKLINK_V4)) > 1
#error "Multiple TrekLink version flags defined. Only one of TREKLINK_V2, TREKLINK_V3, TREKLINK_V4 may be set."
#endif

// ========================================================================
// 2. Per-variant GPIO validation
// ========================================================================

// --- v1.0 (default, no version flag) ---
#if !defined(TREKLINK_V2) && !defined(TREKLINK_V3) && !defined(TREKLINK_V4)
#ifndef BUTTON_PIN_SOS
#error "TrekLink v1.0 requires BUTTON_PIN_SOS (dedicated SOS button)"
#endif
#ifndef PIN_BUZZER
#error "TrekLink v1.0 requires PIN_BUZZER (buzzer for SOS alarm)"
#endif
#endif

// --- v2.0 (Custom PCB, ESP32-S3) ---
#ifdef TREKLINK_V2
#ifndef BUTTON_PIN_SOS
#error "TrekLink v2.0 requires BUTTON_PIN_SOS (dedicated SOS button)"
#endif
#ifndef PIN_BUZZER
#error "TrekLink v2.0 requires PIN_BUZZER (buzzer for SOS alarm)"
#endif
#endif

// --- v3.0 (T-Beam V1.2, COTS) ---
#ifdef TREKLINK_V3
#ifdef BUTTON_PIN_SOS
#error "TrekLink v3.0 must NOT define BUTTON_PIN_SOS (uses gesture-based SOS)"
#endif
#ifndef BUTTON_PIN
#error "TrekLink v3.0 requires BUTTON_PIN (user button for gesture SOS)"
#endif
#endif

// --- v4.0 (T-Beam Supreme, COTS) ---
#ifdef TREKLINK_V4
#ifdef BUTTON_PIN_SOS
#error "TrekLink v4.0 must NOT define BUTTON_PIN_SOS (uses gesture-based SOS)"
#endif
#ifndef BUTTON_PIN
#error "TrekLink v4.0 requires BUTTON_PIN (user button for gesture SOS)"
#endif
#endif

// ========================================================================
// 3. Common TrekLink requirements
// ========================================================================

#ifndef I2C_SDA
#error "TrekLink requires I2C_SDA (I2C bus for OLED/sensors)"
#endif
#ifndef I2C_SCL
#error "TrekLink requires I2C_SCL (I2C bus for OLED/sensors)"
#endif

// ========================================================================
// 4. Variant name string for boot-time logging
// ========================================================================

#if defined(TREKLINK_V4)
#define TREKLINK_VARIANT_NAME "TrekLink v4.0 (T-Beam Supreme)"
#elif defined(TREKLINK_V3)
#define TREKLINK_VARIANT_NAME "TrekLink v3.0 (T-Beam V1.2)"
#elif defined(TREKLINK_V2)
#define TREKLINK_VARIANT_NAME "TrekLink v2.0 (Custom PCB S3)"
#else
#define TREKLINK_VARIANT_NAME "TrekLink v1.0 (Custom PCB)"
#endif

#endif // TREKLINK_VARIANT
