/*
 * TrekLink v2 board start-up (specs/architecture design §3.1).
 * Compiled only by the treklink-v2 env, through its build_src_filter.
 */
#include "Arduino.h"
#include "TrekLink.h"
#include "variant.h"

// Stock weak hook, called at the start of setup() (src/main.cpp:330).
void earlyInitVariant()
{
    // Status LED on (active low) as a boot signal
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Hold the power latch at once; stock drives PIN_POWER_EN high again right after this hook
    pinMode(PIN_POWER_EN, OUTPUT);
    digitalWrite(PIN_POWER_EN, HIGH);

    // GPS power on
    pinMode(GPS_EN_PIN, OUTPUT);
    digitalWrite(GPS_EN_PIN, HIGH);

    // LoRa chip select high, so the SPI bus does not float during boot
    pinMode(LORA_CS, OUTPUT);
    digitalWrite(LORA_CS, HIGH);

    pinMode(POWER_SENSE_PIN, INPUT_PULLUP);
}

// Stock weak hook, called once modules, input, screen and radio are up (src/main.cpp:991).
void lateInitVariant()
{
    treklink::start();
}
