/*
 * TrekLink SOS Helper
 * Centralized SOS logic shared across all TrekLink variants.
 *
 * Callers:
 *   - TrekLinkButtonModule (v1.0/v2.0: dedicated SOS button)
 *   - TrekLinkSOSGesture   (v3.0/v4.0: 3-second hold on user button)
 *   - FallDetectionModule   (auto-SOS after fall pre-alarm timeout)
 *
 * Provides: triggerSOS(), cancelSOS(), broadcastPosition(),
 *           activateAlarms(), deactivateAlarms(), SOS beacon tick.
 */

#pragma once

#include "configuration.h"

// SOS text wire format, defined once. The emitted text is
//   TREKLINK_SOS_TAG " - [lat], [lon]"            e.g. "SOS - [11.123456], [107.654321]"
//   TREKLINK_SOS_FALL_TAG " - [lat], [lon]"       e.g. "SOS - FALL DETECTED - [No GPS]"
// Every SOS text therefore begins with TREKLINK_SOS_TEXT_PREFIX, which the onboard queue classifier
// matches (specs/onboard-queue REQ-EVT-03) and gateway-sync parses. Changing any of these changes the wire format.
#define TREKLINK_SOS_TAG "SOS"
#define TREKLINK_SOS_SEPARATOR " - "
#define TREKLINK_SOS_TEXT_PREFIX TREKLINK_SOS_TAG TREKLINK_SOS_SEPARATOR
#define TREKLINK_SOS_FALL_TAG TREKLINK_SOS_TEXT_PREFIX "FALL DETECTED"

#ifdef TREKLINK_VARIANT

#include <cstdint>

// TrekLink message type discriminators (shared across modules)
#ifndef TREKLINK_MSG_SOS
#define TREKLINK_MSG_SOS  0x01
#endif
#ifndef TREKLINK_MSG_FALL
#define TREKLINK_MSG_FALL 0x02
#endif

class TrekLinkSOSHelper
{
  public:
    static TrekLinkSOSHelper &instance();

    // === Core SOS Actions ===

    /**
     * Trigger SOS mode: send emergency position + text packet,
     * activate local alarms (buzzer, vibrator, LED).
     * @param owner  BuzzerManager owner tag (OWNER_SOS or OWNER_FALL)
     */
    void triggerSOS(uint8_t buzzerOwner);

    /**
     * Cancel active SOS: silence all alarms, reset state.
     * @param owner  BuzzerManager owner tag to release
     */
    void cancelSOS(uint8_t buzzerOwner);

    /**
     * Broadcast current position as a POSITION_APP packet.
     */
    void broadcastPosition();

    /**
     * Activate local alarms (buzzer + vibrator).
     * @param owner  BuzzerManager owner tag
     */
    void activateAlarms(uint8_t buzzerOwner);

    /**
     * Deactivate local alarms (buzzer + vibrator + LED).
     * @param owner  BuzzerManager owner tag to release
     */
    void deactivateAlarms(uint8_t buzzerOwner);

    /**
     * Send an SOS text message with GPS coordinates.
     * Format: "SOS - [lat], [lon]" or "SOS - FALL DETECTED - [lat], [lon]"
     * @param prefix  Text prefix (e.g., "SOS" or "SOS - FALL DETECTED")
     */
    void sendSOSTextMessage(const char *prefix = TREKLINK_SOS_TAG);

    /**
     * Tick function for SOS beacon retransmission.
     * Call from runOnce() while SOS is active.
     * Returns true if a beacon was sent this tick.
     * @param sosStartTime   millis() when SOS was first triggered
     * @param lastTxTime     reference to last TX timestamp (updated on send)
     */
    bool tickBeacon(uint32_t sosStartTime, uint32_t &lastTxTime);

    // === Display ===
    void showBanner(const char *message, uint32_t durationMs = 3000);

  private:
    TrekLinkSOSHelper() = default;
    TrekLinkSOSHelper(const TrekLinkSOSHelper &) = delete;
    TrekLinkSOSHelper &operator=(const TrekLinkSOSHelper &) = delete;

    /**
     * Build a high-priority position packet and send it.
     */
    void sendPositionPacket();
};

#endif // TREKLINK_VARIANT
