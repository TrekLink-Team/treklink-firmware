#include "TrekLinkButtonInput.h"
#include "modules/FallDetectionModule.h"
#include "InputBroker.h"

#ifdef TREKLINK_VARIANT

TrekLinkButtonInput *trekLinkButtonInput;

TrekLinkButtonInput::TrekLinkButtonInput() : UpDownInterruptBase("TrekLinkNav") {}

bool TrekLinkButtonInput::init()
{
    // Map button pins to variant.h definitions with fallbacks
#if defined(BUTTON_PIN_UP) && defined(BUTTON_PIN_DOWN) && defined(BUTTON_PIN)
    uint8_t pinUp = BUTTON_PIN_UP;
    uint8_t pinDown = BUTTON_PIN_DOWN;
    uint8_t pinPress = BUTTON_PIN;
#else
    uint8_t pinUp = 32;      // UP button
    uint8_t pinDown = 35;    // DOWN button (input-only, requires external pull-up)
    uint8_t pinPress = 25;   // MENU button
#endif

    // Map to standard InputBroker events for Meshtastic compatibility
    // This ensures TrekLink navigation works globally with all Meshtastic modules
    input_broker_event eventDown = INPUT_BROKER_USER_PRESS;  // DOWN navigation
    input_broker_event eventUp = INPUT_BROKER_ALT_PRESS;     // UP navigation
    input_broker_event eventPressed = INPUT_BROKER_SELECT;   // MENU select
    
    // Long press events (not currently used by TrekLink)
    input_broker_event eventPressedLong = INPUT_BROKER_SHUTDOWN;
    input_broker_event eventUpLong = INPUT_BROKER_UP_LONG;
    input_broker_event eventDownLong = INPUT_BROKER_DOWN_LONG;

    // Initialize using UpDownInterruptBase pattern
    UpDownInterruptBase::init(pinDown, pinUp, pinPress, 
                              eventDown, eventUp, eventPressed, 
                              eventPressedLong, eventUpLong, eventDownLong,
                              TrekLinkButtonInput::handleIntDown,
                              TrekLinkButtonInput::handleIntUp,
                              TrekLinkButtonInput::handleIntPressed);
    
    // Register with InputBroker for global event distribution
    inputBroker->registerSource(this);
    
    LOG_INFO("TrekLinkButtonInput: Initialized (UP=%d, DOWN=%d, MENU=%d)", pinUp, pinDown, pinPress);
    return true;
}

void TrekLinkButtonInput::handleIntDown()
{
    if (trekLinkButtonInput) {
        trekLinkButtonInput->intDownHandler();
    }
}

void TrekLinkButtonInput::handleIntUp()
{
    if (trekLinkButtonInput) {
        trekLinkButtonInput->intUpHandler();
    }
}

void TrekLinkButtonInput::handleIntPressed()
{
    if (trekLinkButtonInput) {
        trekLinkButtonInput->intPressHandler();
    }
}

// WU-6/F24: Override runOnce() to inject any-button fall cancel (thread context, ISR-safe)
int32_t TrekLinkButtonInput::runOnce()
{
    int32_t result = UpDownInterruptBase::runOnce();

    // Any nav button activity during PRE_ALARM cancels fall alarm
#if !defined(ARCH_STM32WL) && !MESHTASTIC_EXCLUDE_I2C
    if (fallDetectionModule && fallDetectionModule->isInPreAlarm()) {
        if (pressDetected || upDetected || downDetected) {
            fallDetectionModule->cancelFallAlarm();
            LOG_INFO("TrekLinkNav: Fall alarm cancelled by nav button");
        }
    }
#endif

    return result;
}

#endif // TREKLINK_VARIANT
