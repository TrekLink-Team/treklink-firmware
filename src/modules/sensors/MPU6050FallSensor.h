/*
 * MPU6050FallSensor — FallSensorInterface adapter for Adafruit MPU6050
 *
 * Used by TrekLink v1.0 (perfboard with external MPU6050 module).
 * Wraps Adafruit_MPU6050 library to provide raw accel/gyro data
 * to the FallDetectionModule via the FallSensorInterface abstraction.
 */

#pragma once

#include "configuration.h"

#if !defined(ARCH_STM32WL) && !MESHTASTIC_EXCLUDE_I2C && __has_include(<Adafruit_MPU6050.h>)

#include "../FallSensorInterface.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

class MPU6050FallSensor : public FallSensorInterface
{
  public:
    MPU6050FallSensor() = default;

    bool init() override;
    bool readAccel(SensorVec3 &out) override;
    bool readGyro(SensorVec3 &out) override;
    const char *sensorName() const override { return "MPU6050"; }

  private:
    Adafruit_MPU6050 mpu;
};

#endif // MPU6050 available
