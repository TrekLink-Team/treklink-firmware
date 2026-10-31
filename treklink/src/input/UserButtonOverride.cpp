/*
 * v3 and v4: the stock user button keeps click and select but loses its long-hold shutdown, so the
 * 5 s SOS hold can never power the unit off (specs/sos REQ-SOS-TRG-01.4, architecture design §4.2 O-12).
 *
 * The env links with -Wl,--wrap=_ZN12ButtonThread10initButtonERK12ButtonConfig. Stock InputBroker::Init()
 * (src/input/InputBroker.cpp) calls ButtonThread::initButton() from another file, so the call reaches
 * this wrapper, which clears longLongPress for the user button and hands the rest to stock unchanged.
 */
#include "Capabilities.h"

#if !defined(TREKLINK_SOS_BUTTON_PIN) && defined(BUTTON_PIN)

#include "input/ButtonThread.h"

extern "C" bool __real__ZN12ButtonThread10initButtonERK12ButtonConfig(ButtonThread *self, const ButtonConfig &config);

extern "C" bool __wrap__ZN12ButtonThread10initButtonERK12ButtonConfig(ButtonThread *self, const ButtonConfig &config)
{
    if (config.pinNumber != BUTTON_PIN || config.longLongPress == INPUT_BROKER_NONE)
        return __real__ZN12ButtonThread10initButtonERK12ButtonConfig(self, config);
    ButtonConfig changed = config;
    changed.longLongPress = INPUT_BROKER_NONE;
    LOG_INFO("TrekLink: user button GPIO %d keeps click and select, no shutdown on a hold (SOS is the 5 s hold)",
             (int)config.pinNumber);
    return __real__ZN12ButtonThread10initButtonERK12ButtonConfig(self, changed);
}

#endif
