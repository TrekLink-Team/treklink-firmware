/*
 * BuzzerManager - Thread-safe buzzer access singleton
 * Prevents LEDC channel 0 conflict between FallDetection and SOS button modules.
 * Uses FreeRTOS mutex for thread safety.
 * SOS Lock: once OWNER_SOS acquires, OWNER_FALL cannot re-acquire until release.
 */

#pragma once

#include "configuration.h"

// BuzzerOwner enum is always available (referenced by SOS/Fall modules on all variants)
enum BuzzerOwner : uint8_t { OWNER_NONE = 0, OWNER_FALL, OWNER_SOS };

#ifdef PIN_BUZZER

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#ifndef BUZZER_LEDC_CHANNEL
#define BUZZER_LEDC_CHANNEL 0
#endif

class BuzzerManager {
  public:
    static BuzzerManager &instance()
    {
        static BuzzerManager inst;
        return inst;
    }

    /**
     * One-time LEDC setup + attach. Call once during module init (not in ISR).
     */
    void init()
    {
        if (initialized)
            return;
        mutex = xSemaphoreCreateMutex();
        ledcSetup(BUZZER_LEDC_CHANNEL, 2700, 8); // 2.7kHz, 8-bit resolution
        ledcAttachPin(PIN_BUZZER, BUZZER_LEDC_CHANNEL);
        ledcWrite(BUZZER_LEDC_CHANNEL, 0); // Start silent
        initialized = true;
        LOG_INFO("BuzzerManager: Initialized on GPIO %d, LEDC ch%d", PIN_BUZZER, BUZZER_LEDC_CHANNEL);
    }

    /**
     * Acquire buzzer for owner. Returns true if acquired.
     * SOS lock: once OWNER_SOS acquires, OWNER_FALL is blocked until SOS releases.
     */
    bool acquire(BuzzerOwner owner)
    {
        if (!initialized || !mutex)
            return false;
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            bool acquired = false;
            if (currentOwner == OWNER_NONE) {
                // SOS lock check: if SOS locked, FALL cannot acquire
                if (owner == OWNER_FALL && sosLocked) {
                    acquired = false;
                } else {
                    currentOwner = owner;
                    if (owner == OWNER_SOS)
                        sosLocked = true;
                    acquired = true;
                }
            } else if (currentOwner == owner) {
                acquired = true; // Already owned
            } else if (owner == OWNER_SOS && currentOwner == OWNER_FALL) {
                // SOS preempts FALL
                currentOwner = OWNER_SOS;
                sosLocked = true;
                acquired = true;
            }
            xSemaphoreGive(mutex);
            return acquired;
        }
        return false;
    }

    /**
     * Release buzzer. Only the current owner can release.
     */
    void release(BuzzerOwner owner)
    {
        if (!initialized || !mutex)
            return;
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (currentOwner == owner) {
                ledcWrite(BUZZER_LEDC_CHANNEL, 0); // Silence on release
                currentOwner = OWNER_NONE;
                if (owner == OWNER_SOS)
                    sosLocked = false;
            }
            xSemaphoreGive(mutex);
        }
    }

    /**
     * Thread-safe PWM write. Only works if caller is current owner.
     */
    void write(uint8_t duty)
    {
        if (!initialized || !mutex)
            return;
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (currentOwner != OWNER_NONE) {
                ledcWrite(BUZZER_LEDC_CHANNEL, duty);
            }
            xSemaphoreGive(mutex);
        }
    }

    bool isOwnedBy(BuzzerOwner owner) const { return currentOwner == owner; }

  private:
    BuzzerManager() : currentOwner(OWNER_NONE), mutex(nullptr), sosLocked(false), initialized(false) {}
    BuzzerManager(const BuzzerManager &) = delete;
    BuzzerManager &operator=(const BuzzerManager &) = delete;

    BuzzerOwner currentOwner;
    SemaphoreHandle_t mutex;
    bool sosLocked;
    bool initialized;
};

#endif // PIN_BUZZER
