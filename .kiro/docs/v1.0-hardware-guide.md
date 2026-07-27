# TrekLink Hardware Assembly Guide

**Version:** 2.1 (Phase 2 - Simplified, No Power Gating)  
**Date:** 2026-02-05  
**Target:** Two (2) complete TrekLink devices

---

## ⚠️ CRITICAL SAFETY WARNINGS

1. **Battery Polarity:** NEVER reverse battery polarity - instant component damage!
2. **Voltage Limits:** ESP32 and Ra-02 are 3.3V ONLY - applying 5V or 7.4V will destroy them
3. **Soldering Safety:** Work in ventilated area, use heat-resistant surface, keep solder away from face
4. **ESD Protection:** Touch grounded metal before handling ESP32/Ra-02 to discharge static
5. **No Spares:** You have components for exactly 2 devices - double-check BEFORE soldering

---

## 📋 Build Strategy Overview

**Sequential Build Process (One Device at a Time):**

**Device 1:**
1. Breadboard prototype (Phase 1: test all connections)
2. Verify firmware flash and basic operation
3. Transfer to perfboard (solder permanent connections)
4. Full functional testing
5. **Keep breadboard version intact until Device 1 validated**

**Device 2:**
1. Disassemble Device 1 breadboard
2. Reuse tested modules for Device 2 breadboard
3. Repeat breadboard → perfboard process
4. Final validation

---

## 🔧 Required Tools & Materials

### Tools
- **Soldering:** Temperature-controlled iron (300-400°C), 60/40 solder (0.8mm), flux
- **Cutting:** Flush-cut pliers (Plato), wire strippers
- **Measurement:** Digital multimeter, calipers
- **Assembly:** Screwdrivers, tweezers, helping hands, anti-static mat
- **Testing:** USB cable (ESP32 programming), 5V phone charger (TP5100)

### Consumables (From BOM)
- 2x 21700 batteries (Samsung 50E 5000mAh)
- 22 AWG solid wire (power rails)
- 26 AWG stranded wire (signal connections)
- Heat shrink tubing, electrical tape

---

## 📦 Component Inventory Checklist

**Core Modules (2 of each):**
- [ ] ESP32-WROOM-32 DevKit (30-pin)
- [ ] Ra-02 SX1278 breakout (433MHz SPI, IPEX connector)
- [ ] GPS Neo-6M with antenna
- [ ] OLED SSD1306 0.96" (I2C)
- [ ] MPU6050 IMU (I2C)

**Power System (2 of each):**
- [ ] TP5100 charger module (1S Li-ion charging)
- [ ] **FINAL:** TPS63802 buck-boost module (3.3V regulator) *Recommended*
- [ ] **INTERIM:** MT3608 boost + Mini360 buck (14-day fallback) *If TPS63802 not arrived*
- [ ] 2x 21700 battery holder (parallel configuration)
- [ ] Power slide switch

**User Interface (Per Device):**
- [ ] 4x tactile buttons (12x12mm)
- [ ] 1x passive buzzer (12mm)
- [ ] 1x coin vibration motor (10mm)
- [ ] 1x blue LED (5mm)

**Passive Components (Per Device):**
- [ ] 5x 0.1µF (100nF) ceramic capacitors
- [ ] 2x 100µF electrolytic capacitors
- [ ] 2x 1000µF electrolytic capacitors
- [ ] 2x 100kΩ resistors (battery divider)
- [ ] 2x 4.7kΩ resistors (I2C pull-ups)
- [ ] 5x 10kΩ resistors (4× button pull-up/down + 1× CHRG pull-up)
- [ ] 1x 1kΩ resistor (vibrator base)
- [ ] 1x 220Ω resistor (LED current limit)
- [ ] 1x 10kΩ resistor (vibrator base pull-down)
- [ ] 2x 1N4001 diodes (flyback protection)
- [ ] 1x S8050-D NPN transistor (vibrator driver)

**Antennas:**
- [ ] 433MHz SMA whip antenna (LoRa)
- [ ] Ceramic patch (included with GPS)

---

## 🔌 PHASE 1: BREADBOARD PROTOTYPE

### Step 1: Power System Assembly

#### 1.1 Battery Pack Preparation

**CRITICAL: Verify Cell Voltage Match**
1. Measure each 21700 cell voltage with multimeter
2. **Both cells MUST be within 0.1V** (e.g., 3.65V and 3.70V is OK)
3. If mismatch > 0.1V, charge higher-capacity cell first

**Assembly:**
```
[Cell 1 +] ──┬── Battery Holder + terminal
             │
[Cell 2 +] ──┘

[Cell 1 -] ──┬── Battery Holder - terminal
             │
[Cell 2 -] ──┘
```

**Verification:**
- Measure voltage across holder terminals: Should read 3.7V ± 0.2V
- Check polarity with multimeter: Red probe = +, Black probe = -

---

#### 1.2 TP5100 Charger Wiring

**Connections:**
```
Battery Holder (+) ──→ TP5100 BAT+
Battery Holder (-) ──→ TP5100 BAT-

USB-C Cable (5V) ──→ TP5100 VIN+ (or use onboard micro-USB)
USB-C GND ──→ TP5100 VIN-
```

**Test:**
1. Plug USB-C charger (5V phone charger)
2. **Red LED should light** (charging active)
3. Wait 1-2 hours (if cells are ~50% charged)
4. **Blue LED should light** (charge complete at 4.2V)

**⚠️ DO NOT PROCEED until green LED appears!**

---

#### 1.3 Power Regulator Setup (CHOOSE ONE)

> **⭐ RECOMMENDED: Option A (TPS63802)** - Professional solution, 27-day battery life  
> **⚙️ FALLBACK: Option B (MT3608 + Mini360 Cascade)** - 14-day interim, 23-day battery life

---

### **OPTION A: TPS63802 Buck-Boost Module (FINAL SOLUTION)** ⭐

**Why TPS63802:**
- ✅ **Industry-leading efficiency:** 90-96%
- ✅ **Ultra-low quiescent current:** 11µA (vs 3mA for generic)
- ✅ **Extended battery life:** 27.7 days deep sleep (vs 4 days generic)
- ✅ **Wide input range:** 1.3V-5.5V (uses battery down to 1.3V)
- ✅ **Professional grade:** Texas Instruments IC
- ✅ **Compact:** 1.4×2.3mm DFN package
- ✅ **Used in commercial Meshtastic devices** (Nano Series)

**Specifications:**
- Input: 1.3V - 5.5V (covers full Li-ion range 3.0V - 4.2V)
- Output: 3.3V adjustable (1.8V - 5.2V range)
- Current: 2A max @ 3.3V (TrekLink uses ~235mA max)
- Efficiency: 90-96% across load range
- Quiescent: 11µA typical
- Protection: Over-temperature, short-circuit, over-voltage

**Wiring (TPS63802 Module):**
```
Battery (+) ──→ TPS63802 VIN+ (could add a switch here)
Battery (-) ──→ TPS63802 GND

TPS63802 VOUT ──→ 3.3V Rail (to ESP32, all modules)
TPS63802 GND ──→ Ground Rail
```

**Configuration Steps:**
1. **Before connecting battery:**
   - Locate adjustment potentiometer on TPS63802 module
   - Some modules are fixed 3.3V (no adjustment needed)
   
2. **Verify output voltage:**
   - Connect battery to VIN+/GND
   - **DO NOT connect ESP32 yet**
   - Measure VOUT with multimeter
   - Should read **3.30V ± 0.05V**
   
3. **If adjustable:**
   - Turn potentiometer to set exactly 3.30V
   - Clockwise = increase, Counter-clockwise = decrease
   - Verify stability (voltage should not drift)
   
4. **Verification:**
   - Battery voltage: 3.0V - 4.2V (full range)
   - Output voltage: **3.30V** (stable)
   - Efficiency: Module should stay cool (minimal heat)

**Expected Performance:**
- Deep sleep power: 15.011mA (ESP32 10mA + GPS 5mA + TPS63802 0.011mA)
- Active power: ~84mA @ 3.3V
- Battery life (10Ah): **666 hours (27.7 days)** deep sleep
- Charge interval: **Once per month**

---

### **OPTION B: MT3608 + Mini360 Cascade (INTERIM SOLUTION)** ⚙️

> **Use this if TPS63802 has not arrived yet (14-day shipping delay)**

**Why Cascade:**
- ✅ **Available immediately:** Use existing MT3608 stock
- ✅ **Safe voltage regulation:** Proper 3.3V output
- ✅ **Good battery life:** 23 days deep sleep (acceptable for testing)
- ✅ **Works full range:** 3.0V - 4.2V battery
- ✅ **Easy to migrate:** Simple swap to TPS63802 later

**How It Works:**
```
Battery (3.0V-4.2V) → MT3608 Boost → 7.8V → Mini360 Buck → 3.3V
                      Stage 1              Stage 2
```

**Efficiency Analysis:**
- MT3608 boost efficiency: ~90% (3.7V → 7.8V)
- Mini360 buck efficiency: ~95% (7.8V → 3.3V)
- **Total cascade efficiency: 85.5%**
- Power loss: ~14.5% (acceptable for interim)

**Required Components:**
- 1× MT3608 boost converter module
- 1× Mini360 buck converter module
- 2× 100µF electrolytic capacitors (inter-stage filtering)
- 4× 10µF ceramic capacitors (input/output decoupling)

**Circuit Diagram:**
```
Battery -> MT3608 -> 7.8V -> Mini360 -> 3.3V
```

**Wiring Steps:**

**Step 1: Configure MT3608 (Boost Stage)**
1. Connect MT3608 input to battery:
   - MT3608 VIN+ ──→ Battery (+) (could add a switch here)
   - MT3608 VIN- ──→ Battery (-)
2. **Adjust MT3608 output to 7.8V:**
   - Measure VOUT with multimeter
   - Turn potentiometer clockwise to increase
   - Set to exactly **7.80V ± 0.1V**
   - Test with LED: should light brightly
3. **Add 100µF capacitor** on MT3608 output:
   - (+) leg to MT3608 VOUT
   - (-) leg to MT3608 GND
4. **Add 10µF ceramic caps:**
   - One on input (VIN+/VIN-)
   - One on output (VOUT/GND)

**Step 2: Configure Mini360 (Buck Stage)**
1. Connect Mini360 input to MT3608 output:
   - Mini360 IN+ ──→ MT3608 VOUT (7.8V point)
   - Mini360 IN- ──→ MT3608 GND
2. **DO NOT connect ESP32 yet!**
3. **Adjust Mini360 output to 3.3V:**
   - Measure OUT+ with multimeter
   - Turn potentiometer counter-clockwise to decrease
   - Set to exactly **3.30V ± 0.05V**
   - Verify stability (should not drift)
4. **Add 100µF capacitor** on Mini360 output:
   - (+) leg to Mini360 OUT+
   - (-) leg to Mini360 OUT-
5. **Add 10µF ceramic caps:**
   - One on input (IN+/IN-)
   - One on output (OUT+/OUT-)

**Step 3: Verification**
1. Measure battery voltage: Should be 3.7V - 4.2V
2. Measure MT3608 output: Should be **7.8V**
3. Measure Mini360 output: Should be **3.30V**
4. Check all ground connections: Must be common star point
5. Feel modules for heat: Should be barely warm (not hot)

**Expected Performance:**
- Deep sleep power: 18.2mA @ 3.7V
- Active power: ~96mA @ 3.7V
- Battery life (10Ah): **551 hours (23 days)** deep sleep
- Charge interval: **Every 3 weeks**

**⚠️ CRITICAL: Capacitor Placement**
- 100µF caps reduce ripple (DC filtering)
- 10µF caps reduce switching noise (AC filtering)
- MUST have correct polarity: Stripe = negative (-)
- Place physically close to module pins

---

### **Migration Path: Cascade → TPS63802**

**When TPS63802 arrives (Day 14):**

1. **Power down device** (disconnect battery)
2. **Remove cascade modules:**
   - Desolder MT3608 entirely
   - Desolder Mini360 entirely
   - Keep 100µF cap on 3.3V rail (reuse)
3. **Install TPS63802:**
   - Solder VIN+ to Battery (+)
   - Solder GND to Battery (-)
   - Solder VOUT to 3.3V rail (where Mini360 OUT+ was)
   - Solder GND to Ground rail
4. **Verify output:** Should read 3.30V ± 0.05V
5. **Reconnect ESP32:** Test all modules
6. **Done!** Battery life now 27.7 days (from 23 days)

**Saved Components (for future projects):**
- MT3608 module (reusable for other 5V/9V/12V projects)
- Mini360 module (reusable for 3.3V/5V regulation)
- 100µF + 10µF caps (reusable)

---

### **Power Supply Comparison Summary**

| Feature | TPS63802 (Final) | MT3608+Mini360 (Interim) |
|---------|------------------|-------------------------|
| **Efficiency** | 90-96% ⭐ | 85.5% |
| **Quiescent** | 11µA ⭐ | 1.15mA |
| **Battery Life** | **27.7 days** ⭐ | 23 days |
| **Components** | 1 module ⭐ | 2 modules + 6 caps |
| **Size** | Tiny (1.4×2.3mm) ⭐ | Large (~50×30mm) |
| **Cost** | 31,500₫ | ~24,000₫ ⭐ |
| **Availability** | Import (14 days) | Immediate ⭐ |
| **Complexity** | Low ⭐ | Medium |
| **Recommended For** | Final production ⭐ | Testing/prototyping |

**Recommendation:** Use MT3608+Mini360 for immediate testing, migrate to TPS63802 for final build.

---

#### 1.4 Power Rail Capacitors

**0.1µF Ceramic Caps (5 total):**
- Will be placed later at each module (Ra-02, ESP32, GPS, OLED, MPU6050)
- No polarity - can be soldered either direction

**100µF Electrolytic (2 total):**

**Location 1: Battery Input**
```
Battery (+) ──┬── 100µF (+) leg (longer leg)
              │
             GND ──── 100µF (-) leg (shorter leg, white stripe)
```

**Location 2: Mini360 Output**
```
3.3V Rail ──┬── 100µF (+) leg
            │
           GND ──── 100µF (-) leg
```

**1000µF Electrolytic (2 total):**
- Same locations as 100µF (connect in parallel)
- (+) leg to positive rail, (-) leg to GND
- **Check polarity:** Stripe on can = negative side

---

### Step 2: ESP32 + Ra-02 LoRa Module

#### 2.1 Ra-02 SPI Wiring

**Pin Connections (Ra-02 → ESP32):**

| Ra-02 Pin | ESP32 GPIO | Wire Color (Suggested) |
|-----------|------------|------------------------|
| **3.3V** | 3V3 (from Mini360) | Red |
| **GND** | GND | Black |
| **SCK** | GPIO 5 | Yellow |
| **MISO** | GPIO 19 | Green |
| **MOSI** | GPIO 27 | Blue |
| **CS (NSS)** | GPIO 18 | Orange |
| **DIO0** | GPIO 26 | Purple |
| **RESET (RST)** | GPIO 14 | White |

**0.1µF Ceramic Cap:**
- Solder across Ra-02 3.3V and GND pins (as close to module as possible)

**Antenna:**
- Connect 433MHz SMA whip antenna to IPEX connector on Ra-02
- Ensure antenna is NOT touching metal objects

**⚠️ CRITICAL:**
- **NEVER power on Ra-02 without antenna!** (can damage RF amplifier)
- DIO0 **MUST** be connected (required for RX/TX interrupts)

---

#### 2.2 ESP32 Power Connection

**From Mini360:**
```
Mini360 OUT+ ── ESP32 3V3 pin
Mini360 OUT- ── ESP32 GND pin
```

**0.1µF Ceramic Cap:**
- Solder across ESP32 3V3 and GND pins (near the chip)

**Test:**
1. Turn on power switch
2. ESP32 blue LED should light
3. Measure ESP32 3V3 pin: Should read 3.30V

---

### Step 3: GPS Neo-6M (UART)

**Pin Connections (GPS → ESP32):**

| GPS Pin | ESP32 GPIO | Notes |
|---------|------------|-------|
| **VCC** | 3.3V | From Mini360 output |
| **GND** | GND | Common ground |
| **TX** | GPIO 17 (RX1) | GPS transmits to ESP32 |
| **RX** | GPIO 16 (TX1) | ESP32 transmits to GPS |

**0.1µF Ceramic Cap:**
- Solder across GPS VCC and GND

**Antenna Placement:**
- Ceramic patch antenna should face upward (toward sky)
- Keep away from LoRa antenna (>5cm separation)

---

### Step 4: OLED + MPU6050 (I2C Shared Bus)

#### 4.1 I2C Pull-Up Resistors

**CRITICAL: Install BEFORE connecting modules**

```
3.3V Rail ──┬── 4.7kΩ ──→ ESP32 GPIO 21 (SDA)
            │
            └── 4.7kΩ ──→ ESP32 GPIO 22 (SCL)
```

**Why:** I2C requires external pull-ups for reliable communication

---

#### 4.2 OLED SSD1306 Wiring

| OLED Pin | ESP32 GPIO |
|----------|------------|
| **VCC** | 3.3V |
| **GND** | GND |
| **SCL** | GPIO 22 |
| **SDA** | GPIO 21 |

**0.1µF Ceramic Cap:** Across OLED VCC and GND

---

#### 4.3 MPU6050 IMU Wiring

| MPU6050 Pin | ESP32 GPIO | Notes |
|-------------|------------|-------|
| **VCC** | 3.3V | |
| **GND** | GND | |
| **SCL** | GPIO 22 | Shared with OLED |
| **SDA** | GPIO 21 | Shared with OLED |
| **AD0** | GND | Sets I2C address to 0x68 |

**0.1µF Ceramic Cap:** Across MPU6050 VCC and GND

**I2C Address Verification:**
- OLED: 0x3C (default)
- MPU6050: 0x68 (when AD0 = GND)
- No address conflict ✅

---

### Step 5: User Interface (4 Buttons)

**Button Wiring:**

| Button | ESP32 GPIO | Resistor | Resistor Connection |
|--------|------------|----------|---------------------|
| **MENU** | GPIO 25 → GND | 10kΩ | GPIO 25 → 10kΩ → GND (pull-down) |
| **SOS** | GPIO 34 → GND | 10kΩ | 3.3V → 10kΩ → GPIO 34 (pull-up) |
| **UP** | GPIO 32 → GND | 10kΩ | GPIO 32 → 10kΩ → GND (pull-down) |
| **DOWN** | GPIO 35 → GND | 10kΩ | 3.3V → 10kΩ → GPIO 35 (pull-up) |

**Explanation:**
- Button pressed = GPIO reads LOW (0V)
- Button released = GPIO reads HIGH (3.3V via pull-up) or LOW (via pull-down)
- GPIO 34, 35 are **input-only** (need external pull-ups)

---

### Step 6: Notifications (Buzzer, Vibrator, LED)

#### 6.1 Passive Buzzer (GPIO 33)

**Wiring:**
```
ESP32 GPIO 33 ──→ Buzzer (+) positive terminal
                    │
                ┌───┴───────┐
              Buzzer Coil
                └───┬───────┘
                    │
                   GND ──→ Buzzer (-) negative terminal
                    
Flyback Diode (1N4001):
  Buzzer (+) ──→ Cathode (stripe/white end)
  Buzzer (-) ──→ Anode (non-stripe end)
```

**Diode Orientation:**
- **Stripe (white band) connects to Buzzer +** (GPIO 33 side)
- **Non-stripe connects to GND**
- Diode is **parallel** to buzzer, NOT in series!

**Purpose:** Protects ESP32 from voltage spikes when buzzer stops

---

#### 6.2 Vibration Motor (GPIO 4 + NPN Transistor)

**Components:**
- 1x S8050-D NPN transistor (TO-92)
- 1x 1kΩ resistor (base current limit)
- 1x 10kΩ resistor (base pull-down)
- 1x 1N4001 diode (flyback protection)

**NPN Transistor Pinout (S8050-D, TO-92):**
```
Flat side facing you, pins pointing down:
  [Emitter] [Base] [Collector]
     (E)      (B)      (C)
```

**Wiring:**
```
ESP32 GPIO 4 ──→ 1kΩ ──→ NPN Base
                            │
                        10kΩ
                            │
                           GND ──→ NPN Emitter

3.3V ──→ Motor (+)
 
NPN Collector ──→ Motor (-)

Flyback Diode (1N4001):
  Motor (+) ──→ Cathode (stripe)
  Motor (-) ──→ Anode (non-stripe)
```

**How It Works:**
- GPIO 4 HIGH → NPN on → Motor runs
- GPIO 4 LOW → NPN off → Motor stops
- Diode prevents voltage spike from motor coil

---

#### 6.3 Status LED (GPIO 2)

**Wiring:**
```
ESP32 GPIO 2 ──→ 220Ω ──→ LED Anode (+, longer leg)
                           │
                          GND ──→ LED Cathode (-, shorter leg, flat side)
```

**LED Polarity:**
- **Anode (+):** Longer leg, connects to GPIO 2 (via 220Ω)
- **Cathode (-):** Shorter leg, flat side of LED, connects to GND

---

### Step 7: Battery Voltage Monitoring

**Purpose:** Allows the ESP32 to measure actual battery voltage (3.0V–4.2V) via a voltage divider that scales it down to a safe ADC range (1.5V–2.1V).

**Components:**
- 2× 100kΩ resistors (R1 and R2)
- 1× 0.1µF ceramic capacitor (optional, noise filter)

> **Important:** On your ESP32 DevKit board, GPIO 36 is labeled **VP** (or SVP) on the silkscreen — NOT "D36".
> It is an input-only ADC1 pin. Do NOT use ADC2 pins — they conflict with WiFi/BLE.

**Step-by-step wiring:**

1. **Solder R1 (100kΩ) — one leg to Battery (+) terminal.** This is the same positive point that feeds the TP5100 and TPS63802/Mini360.

2. **Connect R1's other leg to ESP32 VP pin** using a short signal wire. This junction point is where the ESP32 reads the divided voltage.

3. **Solder R2 (100kΩ) — one leg to the same junction** (where R1 meets the VP wire from step 2).

4. **Solder R2's remaining leg to GND** (common ground rail).

5. **(Optional) Add a 0.1µF ceramic cap** from **VP pin to GND** — reduces ADC noise.

**Resulting circuit (for reference):**
```
Battery (+)
    │
   [R1] 100kΩ
    │
    ├──── VP pin (GPIO 36) ← ESP32 reads here
    │
   [R2] 100kΩ
    │
   GND
```

**What you should measure with a multimeter at VP:**
- Battery at 4.2V → VP reads **2.1V**
- Battery at 3.7V → VP reads **1.85V**
- Battery at 3.0V → VP reads **1.5V**

**Firmware:** `ADC_MULTIPLIER = 2.0` in variant.h. Meshtastic automatically converts the divided voltage back to actual battery voltage and displays battery %.

---

### Step 7.5: TP5100 Charge Status Detection

**Purpose:** Allows the firmware to detect if the battery is currently charging and show ⚡ icon in the Meshtastic app. Without this, the app can only guess charging state from voltage.

**Components:**
- 1× 10kΩ resistor (pull-up)

> **Background:** The TP5100's CHRG pin is **open-drain** — it works like a switch to GND.
> When charging, the switch closes → line goes to 0V. When idle, the switch opens → line floats.
> The pull-up resistor gives the floating state a defined HIGH (3.3V) so GPIO 13 reads cleanly.

**Step-by-step wiring:**

1. **Solder the 10kΩ resistor — one leg to the 3.3V rail** (the regulated output from TPS63802/Mini360, NOT battery voltage).

2. **Solder the resistor's other leg to ESP32 GPIO 13.** This creates the pull-up — GPIO 13 now defaults to reading 3.3V (HIGH) when nothing pulls it down.

3. **Run a signal wire from the same GPIO 13 point to the TP5100 CHRG pin** on the charger module. That's it — three connections total.

**Resulting circuit (for reference):**
```
3.3V Rail
    │
   [R] 10kΩ (pull-up)
    │
    ├──── ESP32 GPIO 13 ← firmware reads this pin
    │
TP5100 CHRG pin (open-drain: pulls LOW when charging)
```

**What you should measure with a multimeter at GPIO 13:**

| Scenario | GPIO 13 Voltage | TP5100 LED | App Shows |
|----------|----------------|------------|-----------|
| USB in, charging | **~0V** (LOW) | Red on | "Charging ⚡" |
| Charge complete | **~3.3V** (HIGH) | Blue on | "Full" |
| No USB | **~3.3V** (HIGH) | Off | Idle |
| No battery, USB only | **~3.3V** (HIGH) | Flashing | "No battery" |

**Verification steps:**
1. Connect USB charger with battery installed
2. Measure GPIO 13: should read **~0V** (charging)
3. Wait for blue LED on TP5100 (charge complete)
4. Measure GPIO 13: should read **~3.3V** (full)
5. Remove battery, keep USB: GPIO 13 should still read **~3.3V** (no false positive)

---

## ✅ PHASE 1 TESTING: Breadboard Validation

### Test 1: Power System Check

**Procedure:**
1. Fully charge battery (TP5100 green LED)
2. Turn on power switch
3. Measure voltages:
   - Battery terminals: 4.2V
   - Mini360 output: 3.30V
   - ESP32 3V3 pin: 3.30V

**Expected:** All voltages within ±0.05V

---

### Test 2: ESP32 Firmware Flash

**Connect ESP32 to PC via USB cable**

**Commands:**
```bash
cd d:\DATA\Github\treklink
pio run -e treklink -t upload
pio device monitor -b 115200
```

**Expected Serial Output:**
```
[INFO] Meshtastic 2.7.19
[INFO] Region: EU_433
[INFO] Radio: SX1278 initialized
[INFO] Frequency: 433.175 MHz
[INFO] GPS: Neo-6M detected
[INFO] OLED: SSD1306 0x3C found
[INFO] MPU6050: 0x68 detected
[INFO] Battery: 4.2V (100%)
```

**If Errors:**
- "Radio init failed" → Check Ra-02 SPI wiring, verify antenna connected
- "GPS not found" → Check TX/RX swap, verify 3.3V power
- "OLED not found" → Check I2C pull-ups, verify address 0x3C
- "MPU6050 not found" → Check AD0 pin to GND

---

### Test 3: Button Response

**Open serial monitor, press each button:**
- MENU → Log: "Button MENU pressed"
- SOS → Log: "Button SOS pressed"
- UP → Log: "Button UP pressed"
- DOWN → Log: "Button DOWN pressed"

---

### Test 4: GPS Fix Acquisition

**Move device near window (clear sky view)**

**Wait 5-10 minutes for cold start**

**Expected Serial Output:**
```
[INFO] GPS: Acquiring satellites (3 visible)
[INFO] GPS: Fix acquired (8 satellites)
[INFO] Position: Lat 37.7749, Lon -122.4194
```

**OLED Should Show:**
- GPS satellite icon (solid when locked)
- Lat/Lon coordinates

---

### Test 5: LoRa Mesh Communication

**Requires second Meshtastic device (any variant)**

1. Set both devices to **same region** (EU_433)
2. Set both devices to **same channel** (use Meshtastic app)
3. Send test message from Device A
4. Verify Device B receives message
5. Check RSSI/SNR values

**Expected:**
- Message delivery within 1-2 seconds
- RSSI: -50 to -100 dBm (closer = stronger)
- SNR: 5-15 dB (good signal quality)

---

## 🔨 PHASE 2: PERFBOARD SOLDERING

**⚠️ ONLY PROCEED AFTER BREADBOARD FULLY VALIDATED**

### Pre-Soldering Checklist

- [ ] All breadboard tests passed
- [ ] Firmware flashes successfully
- [ ] GPS acquires fix reliably
- [ ] LoRa communication confirmed
- [ ] Buttons respond correctly
- [ ] Battery monitoring shows accurate %

### LED Power Saving (Optional)

**Purpose:** Remove power indicator LEDs to save ~15mA (15-20% battery life)

**Modules to Modify:**
- ESP32 DevKit (red power LED)
- Ra-02 breakout (blue/red LED)
- GPS Neo-6M (red power LED only, **keep green PPS LED**)
- Mini360 (red LED)
- MPU6050 (red LED if present)

**Procedure:**
1. Locate power LED on each module (usually labeled "PWR" or "VCC")
2. Use flush-cut pliers to **snip LED's anode leg** (longer leg)
3. Cut at PCB surface level
4. **DO NOT desolder** (no reflow station needed)

**LEDs to KEEP:**
- TP5100 charge status LEDs (red/green)
- GPS PPS LED (blinks when locked)
- Blue status LED (GPIO 2, firmware-controlled)

---

### Perfboard Layout Tips

**Component Placement Strategy:**

**Power Section (Top-Left):**
- Battery holder
- TP5100 charger (with CHRG pin wire to GPIO 13 + 10kΩ pull-up)
- Power switch
- TPS63802/Mini360 regulator
- Bulk capacitors (100µF, 1000µF)

**Main Board (Center):**
- ESP32 (center position)
- Ra-02 (right of ESP32, near SMA antenna connector edge)
- GPS (left of ESP32)
- OLED (top edge, visible through enclosure)
- MPU6050 (near ESP32 for short I2C lines)

**User Interface (Bottom Edge):**
- 4 buttons in a row
- Buzzer and vibrator nearby

**Wiring Strategy:**
- Use 22 AWG solid wire for power rails (red = +, black = GND)
- Use 26 AWG stranded wire for signals (flexible, easier routing)
- Route GND and 3.3V as continuous bus bars along perfboard edges

---

## 🔧 Troubleshooting Guide

| Symptom | Probable Cause | Fix |
|---------|----------------|-----|
| ESP32 won't boot | Mini360 voltage ≠ 3.3V | Re-adjust potentiometer |
| Ra-02 init failed | Antenna not connected | Connect antenna BEFORE powering on |
| GPS no fix | Indoor testing / obstructed sky | Move outdoors, wait 10 min |
| OLED blank | Missing I2C pull-ups | Add 4.7kΩ to SDA/SCL |
| MPU6050 not detected | AD0 floating | Connect AD0 to GND |
| Battery shows 0% | Voltage divider wiring error | Verify GPIO 36 connected to resistor junction |
| Buttons don't respond | Wrong pull-up/down | Check GPIO 34/35 have pull-ups, GPIO 25/32 have pull-downs |
| Vibrator motor won't run | NPN transistor reversed | Verify S8050-D pinout: E-B-C |
| Buzzer silent | No PWM signal | Check GPIO 33 connection, verify firmware |

---

## 📐 Final Assembly Notes

**Device 1 Timeline:**
- Breadboard: 2-3 days (testing + debugging)
- Perfboard: 4-6 hours (careful soldering)
- Testing: 1 day (full validation)

**Device 2 Timeline:**
- Faster (reuse validated design from Device 1)
- Breadboard: 1 day
- Perfboard: 3-4 hours
- Testing: 4 hours

**Total Time Estimate:** 1 week for both devices

---

## 📞 Getting Help

**If you encounter issues:**
1. Check specification documents:
   - `design.md` - Technical architecture
   - `requirements.md` - EARS requirements
   - `BOM_Development.md` - Component list
2. Review variant.h GPIO definitions
3. Search Meshtastic Discord for similar hardware issues

---

**Document Version:** 2.1  
**Last Updated:** 2026-02-05  
**Author:** TrekLink Development Team
