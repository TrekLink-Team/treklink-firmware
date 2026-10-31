/*
 * TrekLink v2 custom board (ESP32-S3, E22-400M22S SX1268 433 MHz, NEO-M9N, ICM-20948).
 * Parts and capabilities: specs/variants/v2/components.md. Overlay rules: specs/architecture.
 *
 * GPIO Configuration (Frozen per PCB netlist):
 * - I2C: SDA=5, SCL=6
 * - GPS UART: RX=16, TX=17, PWR_EN=15
 * - LoRa SX1268 SPI: SCK=21, MOSI=38, MISO=39, CS=14, DIO1=42, BUSY=41, NRST=40, RXEN=43
 * - Buttons: SELECT=0, SOS=4, UP=7, DOWN=8, POWER_SENSE=13 (Active-Low)
 * - Power: LATCH=9 (Active-High, stock PIN_POWER_EN), BATTERY_PIN=1 (ADC)
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
#define BUTTON_PIN 0                 // MENU: the stock user button (short next, long select)
#define TREKLINK_SOS_BUTTON_PIN 4    // Dedicated SOS button, owned by TrekLinkButtonModule
#define POWER_SENSE_PIN 13           // Power button sense (Active-Low)

// UP and DOWN through the stock two-way rocker input (specs/architecture design §4.2 O-06)
#define INPUTDRIVER_ENCODER_TYPE 2
#define INPUTDRIVER_TWO_WAY_ROCKER 1
#define INPUTDRIVER_TWO_WAY_ROCKER_LEFT 7  // UP
#define INPUTDRIVER_TWO_WAY_ROCKER_RIGHT 8 // DOWN

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
#define TREKLINK_SOS_LED_PIN LED_PIN

// Power latch: stock drives PIN_POWER_EN high at boot and low at shutdown (design §4.2 O-05)
#define PIN_POWER_EN 9       // TPS63802 enable, active high

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

// Region VN 433, which is the regulatory MY_433 band (design §4.2 O-03)
#define REGULATORY_LORA_REGIONCODE meshtastic_Config_LoRaConfig_RegionCode_MY_433

// Default compass orientation — adjust to match ICM-20948 mounting rotation on PCB
#define COMPASS_ORIENTATION meshtastic_Config_DisplayConfig_CompassOrientation_DEGREES_0

// TrekLink capabilities (specs/architecture design §3.2)
#define TREKLINK_VARIANT_NAME "TrekLink v2 (custom S3)"
#define TREKLINK_FALL_SENSOR_ICM20948 1
