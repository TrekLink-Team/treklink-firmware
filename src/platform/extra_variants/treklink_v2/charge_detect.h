#pragma once

// TrekLink v2: external power and charge state inferred from the battery voltage.
//
// The v2 board leaves the BQ24074 CHG and PGOOD outputs unconnected and does not route VBUS to the MCU (KiCad board
// and fab netlist, see docs/hardware/v2/charge-detection.md), so the only charge information the firmware has is the
// cell voltage on BATTERY_PIN. Connecting or removing a charger moves that voltage by 20 to 36 mV within one sample.

#if defined(TREKLINK_V2) && defined(TREKLINK_CHARGE_FROM_VOLTAGE)
void treklinkChargeDetectBegin();
bool treklinkExternalPower();
bool treklinkCharging();
#endif
