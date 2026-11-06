/*
 * QMI8658FallSensor: FallSensorInterface adapter for QMI8658 (via SensorLib)
 *
 * Used by TrekLink v4.0 (LilyGo T-Beam Supreme with onboard QMI8658 IMU).
 * The QMI8658 is on SPI, sharing the SD card bus (SCK 36, MISO 37, MOSI 35) with chip select
 * IMU_CS 34 (variants/esp32s3/tbeam-s3-core/pins_arduino.h:30-37).
 */

#pragma once

#include "configuration.h"

#if !defined(ARCH_STM32WL) && !MESHTASTIC_EXCLUDE_I2C && defined(TREKLINK_V4) && __has_include(<SensorQMI8658.hpp>)

#include "../FallSensorInterface.h"
#include <SPI.h>
#include <SensorQMI8658.hpp>
#include <Wire.h>

class QMI8658FallSensor : public FallSensorInterface
{
  public:
    QMI8658FallSensor() = default;

    bool init() override;
    bool readAccel(SensorVec3 &out) override;
    bool readGyro(SensorVec3 &out) override;
    const char *sensorName() const override { return "QMI8658"; }

  private:
    SensorQMI8658 qmi;
    uint32_t lastReadMs = 0;
    // Cached readings (updated in updateSensorData)
    float accelX = 0, accelY = 0, accelZ = 0;
    float gyroX = 0, gyroY = 0, gyroZ = 0;
    void updateSensorData();
};

#endif // QMI8658 available
