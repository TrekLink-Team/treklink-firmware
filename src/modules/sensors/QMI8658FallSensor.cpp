/*
 * QMI8658FallSensor Implementation
 * Adapter for QMI8658 (SensorLib) → FallSensorInterface
 *
 * Output units:
 *   - Acceleration: m/s² (SI)
 *   - Gyroscope:    rad/s
 *
 * The QMI8658 on the T-Beam Supreme is on SPI, not I2C: it shares the SD card bus
 * (SCK 36, MISO 37, MOSI 35, SD CS 47) with its own chip select IMU_CS 34
 * (variants/esp32s3/tbeam-s3-core/pins_arduino.h:30-37; LilyGo's QMI8658 example
 * starts it with qmi.begin(SDCardSPI, IMU_CS)).
 */

#include "QMI8658FallSensor.h"
#include "FallSensorUnits.h"

#if !defined(ARCH_STM32WL) && !MESHTASTIC_EXCLUDE_I2C && defined(TREKLINK_V4) && __has_include(<SensorQMI8658.hpp>)

#include "SPILock.h"
#include "configuration.h"
#include "main.h"

#if !defined(IMU_CS) || !defined(SPI_SCK) || !defined(SPI_MISO) || !defined(SPI_MOSI) || !defined(SDCARD_CS)
#error "QMI8658FallSensor needs IMU_CS, SPI_SCK, SPI_MISO, SPI_MOSI and SDCARD_CS from the tbeam-s3-core pins_arduino.h"
#endif

// Bus choice: the same SPIClass instance as the SD card. The IMU sits on the SD card's wires, and a second
// SPIClass on the HSPI host would reconfigure the same peripheral behind the SD driver's back. LoRa is not
// affected: on this board it uses the default SPI (FSPI) on pins 12/13/11 (src/main.cpp:836-844; HW_SPI1_DEVICE,
// which would make src/main.cpp:164 a second HSPI instance, is not defined for tbeam-s3-core).
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI) && defined(SDCARD_USE_SPI1)
extern SPIClass SPI_HSPI; // src/FSCommon.cpp:21
#define IMU_SPI SPI_HSPI
#else
// No stock SD driver is built, so nothing else owns HSPI and the IMU can have the host to itself.
static SPIClass imuSpi(HSPI);
#define IMU_SPI imuSpi
#endif

bool QMI8658FallSensor::init()
{
    // spiLock serialises every SPI user (SD in src/FSCommon.cpp:385, LoRa in src/mesh/RadioLibInterface.cpp:19-30).
    concurrency::LockGuard g(spiLock);

    // Same call as setupSDCard() (src/FSCommon.cpp:386); a no-op when the SD setup already started the bus.
    IMU_SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

    // Keep the SD card deselected so it cannot answer while the IMU is addressed.
    pinMode(SDCARD_CS, OUTPUT);
    digitalWrite(SDCARD_CS, HIGH);

    // No pins passed: SensorLib 0.3.4 then leaves the bus as started above and only drives IMU_CS
    // (SensorQMI8658.hpp:271, platform/arduino/SensorCommArduino_SPI.hpp:62-86).
    if (!qmi.begin(IMU_SPI, IMU_CS)) {
        LOG_WARN("QMI8658FallSensor: Sensor not found on SPI (CS GPIO %d)", IMU_CS);
        return false;
    }

    // Configure accelerometer: ±8g range, 31.25Hz ODR
    // Note: ACC_ODR_LOWPOWER modes require gyroscope disabled, so use normal ODR
    qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_8G, SensorQMI8658::ACC_ODR_31_25Hz);

    // Configure gyroscope: ±512dps range, 28.025Hz ODR (lowest available)
    qmi.configGyroscope(SensorQMI8658::GYR_RANGE_512DPS, SensorQMI8658::GYR_ODR_28_025Hz);

    // Enable accelerometer and gyroscope
    qmi.enableAccelerometer();
    qmi.enableGyroscope();

    LOG_INFO("QMI8658FallSensor: Initialized on SPI, CS GPIO %d (±8g, ±512dps)", IMU_CS);
    return true;
}

void QMI8658FallSensor::updateSensorData()
{
    // Rate limit SPI transactions to once per 10ms
    uint32_t now = millis();
    if (now - lastReadMs >= 10 || lastReadMs == 0) {
        concurrency::LockGuard g(spiLock); // the bus is shared with the SD card (see init)
        // SensorLib 0.3.4 getAccelerometer returns g: raw * accelScales, where
        // accelScales = 8.0 / 32768.0 at ACC_RANGE_8G (SensorQMI8658.hpp:521, 910-923).
        qmi.getAccelerometer(accelX, accelY, accelZ);
        // getGyroscope returns degrees per second: raw * gyroScales, where
        // gyroScales = 512.0 / 32768.0 at GYR_RANGE_512DPS (SensorQMI8658.hpp:598, 975-988).
        qmi.getGyroscope(gyroX, gyroY, gyroZ);
        lastReadMs = now;
    }
}

bool QMI8658FallSensor::readAccel(SensorVec3 &out)
{
    updateSensorData();
    // Cached values are in g (see updateSensorData); a device at rest reads about 1 g on one axis.
    out.x = treklink::fall::gToMps2(accelX);
    out.y = treklink::fall::gToMps2(accelY);
    out.z = treklink::fall::gToMps2(accelZ);
    return true;
}

bool QMI8658FallSensor::readGyro(SensorVec3 &out)
{
    updateSensorData();
    // Cached values are in degrees per second (see updateSensorData).
    out.x = treklink::fall::dpsToRadS(gyroX);
    out.y = treklink::fall::dpsToRadS(gyroY);
    out.z = treklink::fall::dpsToRadS(gyroZ);
    return true;
}

#endif // QMI8658 available
