#include "variant.h"
#include "Arduino.h"

#ifdef TREKLINK_V2
void earlyInitVariant()
{
    // 0. Turn ON the status LED immediately (Active-Low) as a boot/diagnostic signal
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW); // LOW is ON since LED_STATE_ON is 0

    // 1. Latch power HIGH immediately (within first 100ms) to maintain device power
    pinMode(POWER_LATCH_PIN, OUTPUT);
    digitalWrite(POWER_LATCH_PIN, HIGH);

    // 2. Power on the GPS module
    pinMode(GPS_EN_PIN, OUTPUT);
    digitalWrite(GPS_EN_PIN, HIGH);

    // 3. Set LoRa Chip Select HIGH to prevent floating SPI bus issues during boot
    pinMode(LORA_CS, OUTPUT);
    digitalWrite(LORA_CS, HIGH);

    // 4. Configure Power Sense pin as pull-up input
    pinMode(POWER_SENSE_PIN, INPUT_PULLUP);
}
#endif
