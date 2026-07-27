/*
 * TrekLink v2.0 ESP32-S3 Custom Variant
 * Hardware: Custom PCB (ESP32-S3 + SX1268 433MHz + ICM-20948 IMU)
 * Version: 1.0 (Meshtastic Fork)
 * Date: 2026-06-15
 *
 * GPIO Configuration (Frozen per PCB netlist):
 * - I2C: SDA=5, SCL=6
 * - GPS UART: RX=16, TX=17, PWR_EN=15
 * - LoRa SX1268 SPI: SCK=21, MOSI=38, MISO=39, CS=14, DIO1=42, BUSY=41, NRST=40, RXEN=43
 * - Buttons: SELECT=0, SOS=4, UP=7, DOWN=8, POWER_SENSE=13 (Active-Low)
 * - Power: LATCH=9 (Active-High), BATTERY_PIN=1 (ADC)
 * - Peripherals: BUZZER=11, VIBRATOR=12, LED=2
 */

#pragma once

// I2C Bus (OLED + ICM-20948 IMU)
#define I2C_SDA 5
#define I2C_SCL 6

// Display Configuration
#define HAS_SCREEN 1

// GPS Module (UART1 + Power Enable)
#undef GPS_RX_PIN
#undef GPS_TX_PIN
#define GPS_RX_PIN 16   // ESP32 TX1 -> GPS RX
#define GPS_TX_PIN 17   // GPS TX -> ESP32 RX1
#define GPS_EN_PIN 15   // GPS Power Enable (Active-High)
#define PIN_GPS_EN GPS_EN_PIN
#define GPS_UBLOX

// User Interface Buttons
#define BUTTON_PIN 0         // SELECT/MENU button (Meshtastic default)
#define BUTTON_PIN_SOS 4     // Dedicated SOS button
#define BUTTON_PIN_UP 7      // UP navigation button
#define BUTTON_PIN_DOWN 8    // DOWN navigation button
#define POWER_SENSE_PIN 13   // Power button sense (Active-Low)

// Battery Monitoring
#define BATTERY_PIN 1        // ADC1_CH0 (GPIO 1)
#define ADC_CHANNEL ADC1_GPIO1_CHANNEL
#define ADC_MULTIPLIER 2.0   // Voltage divider ratio

// Notifications
#define PIN_BUZZER 11        // Buzzer pin
#define BUZZER_LEDC_CHANNEL 0 // LEDC channel for buzzer
#define PIN_VIBRATOR 12      // Vibration motor
#define PIN_VIBRATION PIN_VIBRATOR
#define LED_PIN 2            // Notification LED
#define LED_STATE_ON 0       // Active-Low LED

// Power Management Latch
#define POWER_LATCH_PIN 9    // Power Latch pin (Active-High)

// LoRa SX1268 SPI Configuration
#define USE_SX1268
#define SX126X_DIO3_TCXO_VOLTAGE 1.8
#define TCXO_OPTIONAL
#define LORA_SCK 21
#define LORA_MOSI 38
#define LORA_MISO 39
#define LORA_CS 14
#define LORA_RESET 40
#define LORA_DIO1 42         // SX1268 IRQ
#define LORA_DIO2 41         // SX1268 BUSY (maps to SX126X_BUSY)

#define SX126X_CS LORA_CS
#define SX126X_DIO1 LORA_DIO1
#define SX126X_BUSY LORA_DIO2
#define SX126X_RESET LORA_RESET
#define SX126X_RXEN 43       // RF switch RX enable pin

// Default LoRa region for TrekLink (Vietnam/Malaysia 433MHz)
#define REGULATORY_LORA_REGIONCODE meshtastic_Config_LoRaConfig_RegionCode_MY_433

// Default timezone for TrekLink (Vietnam, GMT+7)
#define DEFAULT_TIMEZONE "ICT-7"

// Default compass orientation — adjust to match ICM-20948 mounting rotation on PCB
#define COMPASS_ORIENTATION meshtastic_Config_DisplayConfig_CompassOrientation_DEGREES_0

// TrekLink variant flags
#ifndef TREKLINK_VARIANT
#define TREKLINK_VARIANT
#endif

#ifndef TREKLINK_V2
#define TREKLINK_V2
#endif

// Default canned message list
#define CANNED_MESSAGE_MODULE_MESSAGES_DEFAULT                                                                                     \
    "MEDICAL-HELP|LOST-HELP|SAFE|EVAC|LOW BAT|COMING|FOUND|LOST|"                                                                  \
    "OK|NO|CHECK?|STOP|GO|WAIT ME|COME2ME|TURNBACK|MEET AT|REQST|"                                                                 \
    "N|S|E|W|R|L|TRAIL|CHKPT|CAMP|DANGR|LO|HI|SPPLY|FD|WTER"
