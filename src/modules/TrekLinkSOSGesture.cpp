/*
 * TrekLink SOS Gesture Module Implementation
 * Polls BUTTON_PIN for 3-second hold to trigger/cancel SOS.
 *
 * Coexistence with Meshtastic's OneButton:
 *   - This module monitors the same GPIO via raw digitalRead()
 *   - OneButton processes short press, double press, and its own long-press events
 *   - We only act at the 3-second mark; OneButton's long-press fires at ~1s
 *   - OneButton's long-press-release triggers shutdown after 5s — our 3s threshold
 *     fires first, so the user gets SOS at 3s and must release before 5s to avoid shutdown
 */

#include "TrekLinkSOSGesture.h"

#if defined(TREKLINK_VARIANT) && !defined(BUTTON_PIN_SOS)

#include "TrekLinkSOSHelper.h"
#include "BuzzerManager.h"
#include "FallDetectionModule.h"
#include "configuration.h"
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
#if !defined(ARCH_STM32WL) && !MESHTASTIC_EXCLUDE_I2C
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
            gestureState = GESTURE_PRESSING;
        }
        break;

    case GESTURE_PRESSING:
        if (!buttonPressed) {
            // Released before 3s threshold — let OneButton handle it
            gestureState = GESTURE_IDLE;
        } else if ((now - pressStartTime) >= SOS_GESTURE_HOLD_MS) {
            // 3s threshold reached
            onHoldThreshold();
            gestureState = GESTURE_HOLD_FIRED;
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

    // SOS beacon retransmission while active
    if (sosActive) {
        TrekLinkSOSHelper::instance().tickBeacon(sosStartTime, lastSOSTxTime);

#ifdef LED_PIN
        // 2Hz LED strobe during SOS
        digitalWrite(LED_PIN, (now / 250) % 2);
#endif
    }

    return SOS_GESTURE_POLL_MS; // 20ms poll interval
}

#endif // TREKLINK_VARIANT && !BUTTON_PIN_SOS
