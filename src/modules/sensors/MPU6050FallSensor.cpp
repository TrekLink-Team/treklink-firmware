/*
 * MPU6050FallSensor Implementation
 * Adapter for Adafruit MPU6050 → FallSensorInterface
 */

#include "MPU6050FallSensor.h"

#if !defined(ARCH_STM32WL) && !MESHTASTIC_EXCLUDE_I2C && __has_include(<Adafruit_MPU6050.h>)

#include "configuration.h"
#include "main.h"

bool MPU6050FallSensor::init()
{
    Wire.begin(I2C_SDA, I2C_SCL);

    if (!mpu.begin(0x68)) {
        LOG_WARN("MPU6050FallSensor: Sensor not found at 0x68");
        return false;
    }

    // Configure for fall detection (matches original FallDetectionModule settings)
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);   // ±8g for impact detection
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);         // ±500°/s for rotation
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);      // Low-pass noise filter

    LOG_INFO("MPU6050FallSensor: Initialized (±8g, ±500°/s, 21Hz LP)");
    return true;
}

bool MPU6050FallSensor::readAccel(SensorVec3 &out)
{
    sensors_event_t accel, gyro, temp;
    mpu.getEvent(&accel, &gyro, &temp);

    out.x = accel.acceleration.x;
    out.y = accel.acceleration.y;
    out.z = accel.acceleration.z;
    return true;
}

bool MPU6050FallSensor::readGyro(SensorVec3 &out)
{
    sensors_event_t accel, gyro, temp;
    mpu.getEvent(&accel, &gyro, &temp);

    out.x = gyro.gyro.x;
    out.y = gyro.gyro.y;
    out.z = gyro.gyro.z;
    return true;
}

#endif // MPU6050 available
