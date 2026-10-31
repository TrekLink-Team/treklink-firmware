/*
 * QMI8658FallSensor — FallSensorInterface adapter for QMI8658 (via SensorLib)
 *
 * Used by TrekLink v4.0 (LilyGo T-Beam Supreme with onboard QMI8658 IMU).
 * The QMI8658 is on the secondary I2C bus: SDA=17, SCL=18 (I2C_SDA/I2C_SCL).
 */

#pragma once

#include "Capabilities.h"

#if defined(TREKLINK_FALL_SENSOR_QMI8658) && __has_include(<SensorQMI8658.hpp>)

#include "fall/FallSensorInterface.h"
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
