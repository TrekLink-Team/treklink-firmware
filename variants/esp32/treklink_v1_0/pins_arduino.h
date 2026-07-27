#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "variant.h"  // Must include variant.h first to get macro definitions

// TrekLink ESP32 Pin Definitions
// Based on ESP32-WROOM-32 pinout with custom GPIO assignment

// I2C
#define PIN_WIRE_SDA I2C_SDA
#define PIN_WIRE_SCL I2C_SCL

static const uint8_t SDA = PIN_WIRE_SDA;
static const uint8_t SCL = PIN_WIRE_SCL;

// SPI (LoRa Ra-02)
#define PIN_SPI_SS   LORA_CS
#define PIN_SPI_MOSI LORA_MOSI
#define PIN_SPI_MISO LORA_MISO
#define PIN_SPI_SCK  LORA_SCK

static const uint8_t SS   = PIN_SPI_SS;
static const uint8_t MOSI = PIN_SPI_MOSI;
static const uint8_t MISO = PIN_SPI_MISO;
static const uint8_t SCK  = PIN_SPI_SCK;

// Analog pins
static const uint8_t A0 = 36;
static const uint8_t A3 = 39;
static const uint8_t A4 = 32;
static const uint8_t A5 = 33;
static const uint8_t A6 = 34;
static const uint8_t A7 = 35;
static const uint8_t A10 = 4;
static const uint8_t A11 = 0;
static const uint8_t A12 = 2;
static const uint8_t A13 = 15;
static const uint8_t A14 = 13;
static const uint8_t A15 = 12;
static const uint8_t A16 = 14;
static const uint8_t A17 = 27;
static const uint8_t A18 = 25;
static const uint8_t A19 = 26;

// DAC
static const uint8_t DAC1 = 25;
static const uint8_t DAC2 = 26;

// Touch
static const uint8_t T0 = 4;
static const uint8_t T1 = 0;
static const uint8_t T2 = 2;
static const uint8_t T3 = 15;
static const uint8_t T4 = 13;
static const uint8_t T5 = 12;
static const uint8_t T6 = 14;
static const uint8_t T7 = 27;
static const uint8_t T8 = 33;
static const uint8_t T9 = 32;

#endif /* Pins_Arduino_h */
