/*
 * TrekLink test hooks, see TrekLinkTestHooks.h.
 */
#include "test/TrekLinkTestHooks.h"

#if defined(TREKLINK_TEST_HOOKS) && TREKLINK_TEST_HOOKS

#include "MeshTypes.h"
#include "NodeDB.h"
#include "fall/FallDetectionModule.h"
#include "main.h"
#include <stdlib.h>
#include <string.h>

#ifdef TREKLINK_SOS_BUTTON_PIN
#include "sos/TrekLinkButtonModule.h"
#else
#include "sos/TrekLinkSOSGesture.h"
#endif

static const char *const PREFIX = "tk-test:";

TrekLinkTestHooks::TrekLinkTestHooks()
    : SinglePortModule("TrekLinkTestHooks", meshtastic_PortNum_TEXT_MESSAGE_APP), concurrency::OSThread("TrekLinkTestHooks")
{
    LOG_WARN("TrekLinkTestHooks: DIAGNOSTIC IMAGE, button presses can be injected over USB");
    if (inputBroker)
        inputObserver.observe(inputBroker);
}

int TrekLinkTestHooks::onInput(const InputEvent *event)
{
    if (event)
        LOG_INFO("TrekLinkTestHooks: input event %d from %s", (int)event->inputEvent, event->source ? event->source : "?");
    return 0;
}

bool TrekLinkTestHooks::pinAllowed(int pin)
{
#ifdef TREKLINK_SOS_BUTTON_PIN
    if (pin == TREKLINK_SOS_BUTTON_PIN)
        return true;
#endif
#ifdef BUTTON_PIN
    if (pin == BUTTON_PIN)
        return true;
#endif
#ifdef INPUTDRIVER_TWO_WAY_ROCKER_LEFT
    if (pin == INPUTDRIVER_TWO_WAY_ROCKER_LEFT || pin == INPUTDRIVER_TWO_WAY_ROCKER_RIGHT)
        return true;
#endif
#ifdef INPUTDRIVER_TWO_WAY_ROCKER_BTN
    if (pin == INPUTDRIVER_TWO_WAY_ROCKER_BTN)
        return true;
#endif
    return false;
}

ProcessMessage TrekLinkTestHooks::handleReceived(const meshtastic_MeshPacket &mp)
{
    // Only a packet this node sent to itself, which is what the local client's text to its own node
    // becomes; mesh traffic from other nodes never matches.
    if (!(mp.from == 0 || isFromUs(&mp)) || mp.to != nodeDB->getNodeNum())
        return ProcessMessage::CONTINUE;
    // A packet heard over LoRa, MQTT or UDP never presses a button, even with a forged sender.
    if (mp.transport_mechanism != meshtastic_MeshPacket_TransportMechanism_TRANSPORT_INTERNAL &&
        mp.transport_mechanism != meshtastic_MeshPacket_TransportMechanism_TRANSPORT_API)
        return ProcessMessage::CONTINUE;
    const auto &d = mp.decoded;
    char text[64];
    size_t n = d.payload.size < sizeof(text) - 1 ? d.payload.size : sizeof(text) - 1;
    memcpy(text, d.payload.bytes, n);
    text[n] = 0;
    if (strncmp(text, PREFIX, strlen(PREFIX)) != 0)
        return ProcessMessage::CONTINUE;
    const char *cmd = text + strlen(PREFIX);

    if (strncmp(cmd, "press ", 6) == 0) {
        char *end = nullptr;
        int pin = (int)strtol(cmd + 6, &end, 10);
        long ms = end ? strtol(end, nullptr, 10) : 0;
        if (!pinAllowed(pin) || ms <= 0 || ms > 20000) {
            LOG_WARN("TrekLinkTestHooks: refused %s (pin not a button, or 1 to 20000 ms)", text);
            return ProcessMessage::STOP;
        }
        if (pressPin >= 0)
            pinMode(pressPin, INPUT_PULLUP);
        LOG_INFO("TrekLinkTestHooks: press GPIO %d for %ld ms", pin, ms);
        pinMode(pin, INPUT_PULLUP | OUTPUT_OPEN_DRAIN); // input stays readable, output only ever pulls low
        digitalWrite(pin, LOW);
        pressPin = pin;
        releaseAt = millis() + (uint32_t)ms;
        setIntervalFromNow(ms);
    } else if (strcmp(cmd, "state") == 0) {
        logState();
    } else {
        LOG_WARN("TrekLinkTestHooks: unknown command %s", text);
    }
    return ProcessMessage::STOP;
}

int32_t TrekLinkTestHooks::runOnce()
{
    if (pressPin >= 0 && (int32_t)(millis() - releaseAt) >= 0) {
        pinMode(pressPin, INPUT_PULLUP);
        LOG_INFO("TrekLinkTestHooks: release GPIO %d", pressPin);
        pressPin = -1;
        logState();
    }
    return pressPin >= 0 ? 10 : INT32_MAX;
}

void TrekLinkTestHooks::logState()
{
#ifdef TREKLINK_SOS_BUTTON_PIN
    bool sos = trekLinkButtonModule && trekLinkButtonModule->isSOSActive();
#else
    bool sos = trekLinkSOSGesture && trekLinkSOSGesture->isSOSActive();
#endif
    bool pre = fallDetectionModule && fallDetectionModule->isInPreAlarm();
    bool autoSos = fallDetectionModule && fallDetectionModule->isInSOSTriggered();
    LOG_INFO("TrekLinkTestHooks: state sos=%d fall_prealarm=%d fall_sos=%d", sos, pre, autoSos);
}

#endif
