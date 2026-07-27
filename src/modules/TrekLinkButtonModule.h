/*
 * TrekLink Button Module Header
 * Handles SOS button for TrekLink device emergency functions
 * 
 * Debounce: Clean 4-state machine (IDLE→DEBOUNCE→PRESSED→HOLD_ACTIVE)
 * separates debounce from event detection to prevent swallowed presses.
 * 
 * Note: UP/DOWN/MENU navigation buttons are handled by TrekLinkButtonInput
 * which integrates with Meshtastic's InputBroker system.
 */

#pragma once

#include "configuration.h"

#ifdef TREKLINK_VARIANT

#include "SinglePortModule.h"
#include "concurrency/OSThread.h"

// SOS Button GPIO (from variant.h)
#ifdef BUTTON_PIN_SOS
#define BTN_SOS BUTTON_PIN_SOS
#else
#define BTN_SOS 34   // SOS button (input-only, requires external pull-up)
#endif

// Timing constants (milliseconds)
#define DEBOUNCE_MS 50
#define HOLD_THRESHOLD_MS 3000  // 3 seconds for SOS trigger/cancel


// Confirmation feedback patterns
#define CONFIRM_TRIGGER_PULSES    3   // 3 short pulses on SOS trigger
#define CONFIRM_TRIGGER_ON_MS   100   // Pulse ON duration
#define CONFIRM_TRIGGER_OFF_MS  100   // Pulse OFF duration
#define CONFIRM_CANCEL_PULSES     2   // 2 long pulses on SOS cancel
#define CONFIRM_CANCEL_ON_MS    300   // Pulse ON duration
#define CONFIRM_CANCEL_OFF_MS   200   // Pulse OFF duration
#define CONFIRM_PING_MS          50   // Single short pulse on position ping

// TrekLink message type discriminator (F8)
#define TREKLINK_MSG_SOS   0x01
#define TREKLINK_MSG_FALL  0x02
#define TREKLINK_MSG_PRIV  0x03  // Future private protocol

class TrekLinkButtonModule : public SinglePortModule, private concurrency::OSThread
{
private:
    // Clean 4-state debounce machine
    enum SOSButtonPhase : uint8_t {
        SOS_BTN_IDLE,        // Waiting for press (raw HIGH = idle with pull-up)
        SOS_BTN_DEBOUNCE,    // Debouncing: waiting for stable LOW reading
        SOS_BTN_PRESSED,     // Confirmed pressed, timing hold duration
        SOS_BTN_HOLD_ACTIVE  // Hold threshold exceeded, SOS triggered/active
    };

    // Button state
    SOSButtonPhase btnPhase;
    bool lastStableReading;       // Last confirmed stable reading (after debounce)
    uint32_t debounceStartTime;   // When debounce timer started
    uint32_t pressStartTime;      // When confirmed press started (for hold timing)

    // SOS state tracking
    bool sosActive;
    uint32_t sosStartTime;
    uint32_t lastSOSTxTime;       // SOS beacon retransmission timing


    // Confirmation feedback state machine (non-blocking pulse sequence)
    uint8_t confirmPulsesRemaining;   // Pulses left in current confirmation
    uint32_t confirmPulseOnMs;        // ON duration for current pattern
    uint32_t confirmPulseOffMs;       // OFF duration for current pattern
    uint32_t lastConfirmPulseTime;    // Last pulse transition time
    bool confirmPulseOn;              // Current pulse ON/OFF state

    // Private methods
    void pollButton();                // Core debounce state machine
    void onButtonPress();             // Called on confirmed press
    void onButtonRelease(uint32_t holdDuration);  // Called on confirmed release
    void onHoldThreshold();           // Called when 3s hold reached

    void broadcastPosition();
    void triggerSOS();
    void activateSOSAlarms();
    void cancelSOS();

    void startConfirmation(uint8_t pulses, uint32_t onMs, uint32_t offMs);
    void updateConfirmation();
    void vibratorPulse(bool on);
    void showBanner(const char *message, uint32_t durationMs = 3000);

protected:
    virtual int32_t runOnce() override;

public:
    TrekLinkButtonModule();
    
    // Module lifecycle
    virtual bool wantUIFrame() override { return false; }
    
    // Accessors
    bool isSOSActive() const { return sosActive; }
};

extern TrekLinkButtonModule *trekLinkButtonModule;

#endif // TREKLINK_VARIANT
