/*
 * QMI8658FallSensor Implementation
 * Adapter for QMI8658 (SensorLib) → FallSensorInterface
 *
 * Output units:
 *   - Acceleration: m/s² (SI)
 *   - Gyroscope:    rad/s
 *
 * The QMI8658 on T-Beam Supreme is at I2C address 0x6B (QMI8658_L_SLAVE_ADDRESS),
 * on pins I2C_SDA=17, I2C_SCL=18.
 */

#include "fall/sensors/QMI8658FallSensor.h"

#if defined(TREKLINK_FALL_SENSOR_QMI8658) && __has_include(<SensorQMI8658.hpp>)

#include "Capabilities.h"
#include "main.h"

// QMI8658 I2C address (low address = 0x6B)
#ifndef QMI8658_L_SLAVE_ADDRESS
#define QMI8658_L_SLAVE_ADDRESS 0x6B
#endif

bool QMI8658FallSensor::init()
{
    Wire.begin(I2C_SDA, I2C_SCL);

    // Probe QMI8658 at 0x6B (default for T-Beam Supreme)
    if (!qmi.begin(Wire, QMI8658_L_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
        LOG_WARN("QMI8658FallSensor: Sensor not found at 0x6B");
        return false;
    }

    // +-16 g so an impact peak is not clipped, at an output rate above the 50 Hz the detector samples
    // (specs/fall-detection REQ-FALL-RNG-01); the gyro range is the widest SensorLib 0.3.4 offers.
    qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_16G, SensorQMI8658::ACC_ODR_62_5Hz);
    qmi.configGyroscope(SensorQMI8658::GYR_RANGE_1024DPS, SensorQMI8658::GYR_ODR_56_05Hz);

    // Enable accelerometer and gyroscope
    qmi.enableAccelerometer();
    qmi.enableGyroscope();

    LOG_INFO("QMI8658FallSensor: Initialized (+-16g at 62.5 Hz, +-1024dps at 56 Hz)");
    return true;
}

void QMI8658FallSensor::updateSensorData()
{
    // Rate limit I2C transactions to once per 10ms
    uint32_t now = millis();
    if (now - lastReadMs >= 10 || lastReadMs == 0) {
        // SensorLib 0.3.4 returns g and dps (SensorQMI8658.hpp:917-919 and :982-984)
        qmi.getAccelerometer(accelX, accelY, accelZ);
        qmi.getGyroscope(gyroX, gyroY, gyroZ);
        lastReadMs = now;
    }
}

bool QMI8658FallSensor::readAccel(SensorVec3 &out)
{
    updateSensorData();
    // getAccelerometer() returns g: its scale is range / 32768 (SensorQMI8658.hpp:514-525). The old
    // division by 1000 made every reading 1000 times too small, so v4 always looked in free fall (F16).
    out.x = accelX * 9.80665f;
    out.y = accelY * 9.80665f;
    out.z = accelZ * 9.80665f;
    return true;
}

bool QMI8658FallSensor::readGyro(SensorVec3 &out)
{
    updateSensorData();
    // SensorLib getGyroscope returns dps (degrees per second), convert to rad/s:
    // rad/s = dps * (PI / 180)
    out.x = gyroX * (PI / 180.0f);
    out.y = gyroY * (PI / 180.0f);
    out.z = gyroZ * (PI / 180.0f);
    return true;
}

#endif // QMI8658 available
