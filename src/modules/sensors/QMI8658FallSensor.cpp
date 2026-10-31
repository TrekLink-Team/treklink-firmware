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

#include "QMI8658FallSensor.h"
#include "FallSensorUnits.h"

#if !defined(ARCH_STM32WL) && !MESHTASTIC_EXCLUDE_I2C && defined(TREKLINK_V4) && __has_include(<SensorQMI8658.hpp>)

#include "configuration.h"
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

    // Configure accelerometer: ±8g range, 31.25Hz ODR
    // Note: ACC_ODR_LOWPOWER modes require gyroscope disabled, so use normal ODR
    qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_8G, SensorQMI8658::ACC_ODR_31_25Hz);

    // Configure gyroscope: ±512dps range, 28.025Hz ODR (lowest available)
    qmi.configGyroscope(SensorQMI8658::GYR_RANGE_512DPS, SensorQMI8658::GYR_ODR_28_025Hz);

    // Enable accelerometer and gyroscope
    qmi.enableAccelerometer();
    qmi.enableGyroscope();

    LOG_INFO("QMI8658FallSensor: Initialized (±8g, ±512dps)");
    return true;
}

void QMI8658FallSensor::updateSensorData()
{
    // Rate limit I2C transactions to once per 10ms
    uint32_t now = millis();
    if (now - lastReadMs >= 10 || lastReadMs == 0) {
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
