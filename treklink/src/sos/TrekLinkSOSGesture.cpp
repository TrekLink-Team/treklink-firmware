/*
 * TrekLink SOS Gesture Module Implementation
 * Polls BUTTON_PIN for 3-second hold to trigger/cancel SOS.
 *
 * Coexistence with the stock user button (specs/sos REQ-SOS-TRG-01):
 *   - This module reads the same GPIO with digitalRead(); stock keeps click = next screen and the
 *     0.5 s hold = select, which fires first and tells a person who meant "select" to let go.
 *   - From 3 s a banner says that SOS fires if the hold continues; at 5 s SOS triggers (or cancels).
 *   - The stock shutdown on a long hold is removed for this button (UserButtonOverride.cpp), so the
 *     release after an SOS hold cannot power the unit off.
 */

#include "sos/TrekLinkSOSGesture.h"

#if !defined(TREKLINK_SOS_BUTTON_PIN)

#include "sos/TrekLinkSOSHelper.h"
#include "alarm/BuzzerManager.h"
#include "fall/FallDetectionModule.h"
#include "Capabilities.h"
#include "main.h"

TrekLinkSOSGesture *trekLinkSOSGesture;

TrekLinkSOSGesture::TrekLinkSOSGesture()
    : SinglePortModule("TrekLinkSOSGesture", meshtastic_PortNum_PRIVATE_APP),
      OSThread("TrekLinkSOSGesture"),
      gestureState(GESTURE_IDLE),
      pressStartTime(0),
      cooldownEndTime(0),
      sosActive(false),
      sosStartTime(0),
      lastSOSTxTime(0)
{
    LOG_INFO("TrekLinkSOSGesture: Initialized on BUTTON_PIN=%d (3s hold for SOS)", BUTTON_PIN);
}

void TrekLinkSOSGesture::onHoldThreshold()
{
    // Check if fall detection pre-alarm is active — cancel it instead of toggling SOS
#if TREKLINK_HAS_FALL_DETECTION
    if (fallDetectionModule && fallDetectionModule->isInPreAlarm()) {
        LOG_INFO("SOSGesture: Cancelling fall pre-alarm (3s hold)");
        fallDetectionModule->cancelFallAlarm();
        return;
    }
    if (fallDetectionModule && fallDetectionModule->isInSOSTriggered()) {
        LOG_INFO("SOSGesture: Cancelling auto-SOS (3s hold)");
        fallDetectionModule->cancelAutoSOS();
        return;
    }
#endif

    if (!sosActive) {
        LOG_WARN("SOSGesture: SOS TRIGGERED (3s hold)");
        TrekLinkSOSHelper::instance().triggerSOS(OWNER_SOS);
        sosActive = true;
        sosStartTime = millis();
        lastSOSTxTime = millis();

        TrekLinkSOSHelper::instance().showBanner("SOS ACTIVE", 0);
    } else {
        LOG_INFO("SOSGesture: SOS CANCELLED (3s hold)");
        TrekLinkSOSHelper::instance().cancelSOS(OWNER_SOS);
        sosActive = false;

        TrekLinkSOSHelper::instance().showBanner("SOS Cancelled", 3000);
    }
}

int32_t TrekLinkSOSGesture::runOnce()
{
    // Read raw GPIO state
    // BUTTON_PIN is active-low on T-Beam (internal pull-up)
    bool buttonPressed = (digitalRead(BUTTON_PIN) == LOW);
    uint32_t now = millis();

    switch (gestureState) {

    case GESTURE_IDLE:
        if (buttonPressed) {
            pressStartTime = now;
            warned = false;
            gestureState = GESTURE_PRESSING;
        }
        break;

    case GESTURE_PRESSING:
        if (!buttonPressed) {
            // Released before 3s threshold — let OneButton handle it
            gestureState = GESTURE_IDLE;
        } else if ((now - pressStartTime) >= SOS_GESTURE_HOLD_MS) {
            onHoldThreshold();
            gestureState = GESTURE_HOLD_FIRED;
        } else if (!warned && (now - pressStartTime) >= SOS_GESTURE_WARN_MS) {
            warned = true;
            LOG_INFO("SOSGesture: hold past %u ms, %s at %u ms", (unsigned)SOS_GESTURE_WARN_MS, sosActive ? "cancel" : "SOS",
                     (unsigned)SOS_GESTURE_HOLD_MS);
            TrekLinkSOSHelper::instance().showBanner(sosActive ? "Keep holding\nto cancel SOS" : "Keep holding\nfor SOS", 2500);
        }
        break;

    case GESTURE_HOLD_FIRED:
        // Wait for button release
        if (!buttonPressed) {
            // Enter cooldown to avoid immediate re-trigger
            cooldownEndTime = now + 500; // 500ms cooldown
            gestureState = GESTURE_COOLDOWN;
        }
        break;

    case GESTURE_COOLDOWN:
        if (now >= cooldownEndTime) {
            gestureState = GESTURE_IDLE;
        }
        break;
    }

    // SOS beacon retransmission while active; the LED follows the SOS pattern (AlarmPlayer)
    if (sosActive)
        TrekLinkSOSHelper::instance().tickBeacon(sosStartTime, lastSOSTxTime);

    return SOS_GESTURE_POLL_MS; // 20ms poll interval
}

#endif // !TREKLINK_SOS_BUTTON_PIN
