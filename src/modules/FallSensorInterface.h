/*
 * FallSensorInterface — IMU abstraction for TrekLink Fall Detection
 *
 * Pure virtual interface that decouples FallDetectionModule from
 * any specific IMU library. Each hardware variant provides its own
 * adapter:
 *   - MPU6050FallSensor  (v1.0 perfboard)
 *   - ICM20948FallSensor (v2.0 custom PCB)
 *   - QMI8658FallSensor  (v4.0 T-Beam Supreme)
 *
 * Units convention:
 *   - Acceleration: m/s² (raw SI, caller divides by 9.81 for g-force)
 *   - Gyroscope:    rad/s
 */

#pragma once

#include <cstdint>

/**
 * 3-axis sensor reading (accel or gyro)
 */
struct SensorVec3 {
    float x;
    float y;
    float z;
};

/**
 * Abstract interface for IMU sensors used by FallDetectionModule.
 *
 * Implementors must provide:
 *   - init()      — one-time hardware setup (I2C begin, range config)
 *   - readAccel() — latest accelerometer reading in m/s²
 *   - readGyro()  — latest gyroscope reading in rad/s
 */
class FallSensorInterface
{
  public:
    virtual ~FallSensorInterface() = default;

    /**
     * Initialize the sensor hardware.
     * @return true on success, false if sensor not found / init failed
     */
    virtual bool init() = 0;

    /**
     * Read the latest accelerometer data.
     * @param out  Filled with x/y/z acceleration in m/s²
     * @return true on success
     */
    virtual bool readAccel(SensorVec3 &out) = 0;

    /**
     * Read the latest gyroscope data.
     * @param out  Filled with x/y/z angular velocity in rad/s
     * @return true on success
     */
    virtual bool readGyro(SensorVec3 &out) = 0;

    /**
     * Optional: return a human-readable sensor name for logging.
     */
    virtual const char *sensorName() const { return "Unknown"; }
};
