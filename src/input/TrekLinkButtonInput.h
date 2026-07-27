#pragma once
#include "UpDownInterruptBase.h"

#ifdef TREKLINK_VARIANT

/**
 * TrekLinkButtonInput - 3-Button Navigation for TrekLink
 * 
 * Provides UP/DOWN/MENU navigation buttons for TrekLink variant using
 * Meshtastic's standard InputBroker system. Works globally for all modules
 * (UI menus, CannedMessage, etc.).
 * 
 * Hardware:
 *   - UP button: GPIO 32
 *   - DOWN button: GPIO 35 (input-only, requires external pull-up)
 *   - MENU button: GPIO 25
 * 
 * Note: SOS button (GPIO 34) is handled separately in TrekLinkButtonModule
 * for emergency-specific functions only.
 */
class TrekLinkButtonInput : public UpDownInterruptBase
{
  public:
    TrekLinkButtonInput();
    bool init();
    int32_t runOnce() override; // F24: Override for any-button fall cancel
    
    // ISR handlers (must be static for attachInterrupt)
    static void handleIntDown();   // DOWN button (GPIO 35)
    static void handleIntUp();     // UP button (GPIO 32)
    static void handleIntPressed(); // MENU button (GPIO 25)
};

extern TrekLinkButtonInput *trekLinkButtonInput;

#endif // TREKLINK_VARIANT
