#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID 0x303a
#define USB_PID 0x1001

static const uint8_t TX = 43;
static const uint8_t RX = 44;

// Default Wire interface mapped to OLED + IMU
static const uint8_t SDA = 5;
static const uint8_t SCL = 6;

// Default SPI interface mapped to LoRa SX1268
static const uint8_t SS = 14;
static const uint8_t MOSI = 38;
static const uint8_t MISO = 39;
static const uint8_t SCK = 21;

#endif /* Pins_Arduino_h */
