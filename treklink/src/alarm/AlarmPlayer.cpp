/*
 * AlarmPlayer, see AlarmPlayer.h.
 */
#include "alarm/AlarmPlayer.h"
#include "alarm/SosPattern.h"

AlarmPlayer *alarmPlayer = nullptr;

AlarmPlayer::AlarmPlayer() : concurrency::OSThread("TrekLinkAlarm")
{
#ifdef PIN_BUZZER
    BuzzerManager::instance().init();
#endif
#ifdef PIN_VIBRATOR
    pinMode(PIN_VIBRATOR, OUTPUT);
    digitalWrite(PIN_VIBRATOR, LOW);
#endif
    disable();
}

void AlarmPlayer::startSOS(uint8_t newOwner)
{
    if (playing)
        return;
#ifdef PIN_BUZZER
    BuzzerManager::instance().acquire(static_cast<BuzzerOwner>(newOwner));
#endif
    owner = newOwner;
    playing = true;
    markOn = false;
    startMs = millis();
    LOG_INFO("TrekLinkAlarm: SOS pattern on (owner=%u)", (unsigned)owner);
    setIntervalFromNow(0);
    enabled = true;
}

void AlarmPlayer::stop(uint8_t stopOwner)
{
    if (playing)
        LOG_INFO("TrekLinkAlarm: SOS pattern off (owner=%u)", (unsigned)stopOwner);
    playing = false;
    markOn = false;
    setOutputs(false);
#ifdef PIN_BUZZER
    BuzzerManager::instance().release(static_cast<BuzzerOwner>(stopOwner));
#endif
    disable();
}

int32_t AlarmPlayer::runOnce()
{
    if (!playing)
        return disable();
    const bool on = treklink::alarm::sosMarkAt(millis() - startMs);
    if (on != markOn) {
        markOn = on;
        setOutputs(on);
    }
    return 20;
}

void AlarmPlayer::setOutputs(bool on)
{
#ifdef PIN_BUZZER
    if (on)
        BuzzerManager::instance().mark(128); // set up and attach again: stock tone() may have taken the pin (F2)
    else
        BuzzerManager::instance().write(0);
#endif
#ifdef PIN_VIBRATOR
    if (!vibratorHeld)
        digitalWrite(PIN_VIBRATOR, on ? HIGH : LOW);
#endif
#ifdef TREKLINK_SOS_LED_PIN
    pinMode(TREKLINK_SOS_LED_PIN, OUTPUT);
    digitalWrite(TREKLINK_SOS_LED_PIN, on ? LED_STATE_ON : LED_STATE_OFF);
#endif
}
