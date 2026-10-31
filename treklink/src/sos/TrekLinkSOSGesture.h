/*
 * TrekLink SOS Gesture Module
 * Single-button SOS for T-Beam variants (v3.0/v4.0) that lack a dedicated SOS button.
 *
 * Monitors BUTTON_PIN via raw GPIO polling (20ms interval).
 * - 3-second hold → trigger/cancel SOS via TrekLinkSOSHelper
 * - Short press / double press → NOT consumed, passes through to Meshtastic's OneButton
 *
 * Compiled when the variant has no dedicated SOS button (no TREKLINK_SOS_BUTTON_PIN).
 */

#pragma once

#include "Capabilities.h"

#if !defined(TREKLINK_SOS_BUTTON_PIN)

#include "SinglePortModule.h"
#include "concurrency/OSThread.h"

// Timing constants
#define SOS_GESTURE_HOLD_MS 5000   // specs/sos REQ-SOS-TRG-01: SOS on a 5 s hold
#define SOS_GESTURE_WARN_MS 3000   // banner from 3 s: keep holding for SOS
#define SOS_GESTURE_POLL_MS 20     // 20ms poll interval

class TrekLinkSOSGesture : public SinglePortModule, private concurrency::OSThread
{
  public:
    TrekLinkSOSGesture();

    bool isSOSActive() const { return sosActive; }

  protected:
    virtual int32_t runOnce() override;

  private:
    // Gesture state machine
    enum GestureState : uint8_t {
        GESTURE_IDLE,          // Waiting for button press
        GESTURE_PRESSING,      // Button is down, timing hold duration
        GESTURE_HOLD_FIRED,    // 3s hold threshold reached, waiting for release
        GESTURE_COOLDOWN       // Post-release cooldown to avoid re-triggers
    };

    GestureState gestureState;
    uint32_t pressStartTime;       // When the current press started
    uint32_t cooldownEndTime;      // When cooldown expires

    // SOS state
    bool sosActive;
    bool warned = false; // the 3 s "keep holding" banner was shown for this press
    uint32_t sosStartTime;
    uint32_t lastSOSTxTime;

    // Internal methods
    void onHoldThreshold();
};

extern TrekLinkSOSGesture *trekLinkSOSGesture;

#endif // !TREKLINK_SOS_BUTTON_PIN
