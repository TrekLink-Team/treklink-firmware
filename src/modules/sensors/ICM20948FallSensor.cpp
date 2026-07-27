/*
 * ICM20948FallSensor Implementation
 * Adapter for SparkFun ICM-20948 → FallSensorInterface
 */

#include "ICM20948FallSensor.h"

#if !defined(ARCH_STM32WL) && !MESHTASTIC_EXCLUDE_I2C && __has_include(<ICM_20948.h>)

#include "configuration.h"
#include "main.h"
#include "../../motion/ICM20948Sensor.h"

bool ICM20948FallSensor::init()
{
    // Check if the AccelerometerThread's singleton already initialized the hardware
    // (with magnetometer/compass support). If so, join it — skip the duplicate reset.
    if (ICM20948Singleton::isInitialized()) {
        icm = ICM20948Singleton::GetInstance();
        usingSingleton = true;
        LOG_INFO("ICM20948FallSensor: Joined existing singleton (compass enabled)");

        // Set fall-detection-appropriate full scale on the already-initialized sensor
        ICM_20948_fss_t fss;
        fss.a = gpm8;
        fss.g = dps500;
        if (icm->setFullScale((ICM_20948_Internal_Acc | ICM_20948_Internal_Gyr), fss) != ICM_20948_Stat_Ok) {
            LOG_WARN("ICM20948FallSensor: Full scale range setup failed on singleton");
            return false;
        }

        LOG_INFO("ICM20948FallSensor: Initialized via singleton (±8g, ±500dps)");
        return true;
    }

    // No singleton available — initialize hardware ourselves (fallback, no magnetometer)
    Wire.begin(I2C_SDA, I2C_SCL);

    bool success = false;
    if (fallbackIcm.begin(Wire, true) == ICM_20948_Stat_Ok) {
        success = true;
    } else if (fallbackIcm.begin(Wire, false) == ICM_20948_Stat_Ok) {
        success = true;
    }

    if (!success) {
        LOG_WARN("ICM20948FallSensor: Sensor not found at 0x69 or 0x68");
        return false;
    }

    if (fallbackIcm.swReset() != ICM_20948_Stat_Ok) {
        LOG_WARN("ICM20948FallSensor: Reset failed");
        return false;
    }
    delay(100);

    if (fallbackIcm.sleep(false) != ICM_20948_Stat_Ok) {
        LOG_WARN("ICM20948FallSensor: Wakeup failed");
        return false;
    }

    if (fallbackIcm.lowPower(false) != ICM_20948_Stat_Ok) {
        LOG_WARN("ICM20948FallSensor: Power mode config failed");
        return false;
    }

    if (fallbackIcm.setSampleMode((ICM_20948_Internal_Acc | ICM_20948_Internal_Gyr),
                                   ICM_20948_Sample_Mode_Continuous) != ICM_20948_Stat_Ok) {
        LOG_WARN("ICM20948FallSensor: Continuous mode set failed");
        return false;
    }

    ICM_20948_fss_t fss;
    fss.a = gpm8;
    fss.g = dps500;
    if (fallbackIcm.setFullScale((ICM_20948_Internal_Acc | ICM_20948_Internal_Gyr), fss) != ICM_20948_Stat_Ok) {
        LOG_WARN("ICM20948FallSensor: Full scale range setup failed");
        return false;
    }

    icm = &fallbackIcm;
    usingSingleton = false;
    LOG_INFO("ICM20948FallSensor: Self-initialized (±8g, ±500dps, no compass)");
    return true;
}

void ICM20948FallSensor::updateSensorData()
{
    uint32_t now = millis();
    if (now - lastReadMs >= 10 || lastReadMs == 0) {
        icm->getAGMT();
        lastReadMs = now;
    }
}

bool ICM20948FallSensor::readAccel(SensorVec3 &out)
{
    updateSensorData();
    out.x = (icm->accX() / 1000.0f) * 9.80665f;
    out.y = (icm->accY() / 1000.0f) * 9.80665f;
    out.z = (icm->accZ() / 1000.0f) * 9.80665f;
    return true;
}

bool ICM20948FallSensor::readGyro(SensorVec3 &out)
{
    updateSensorData();
    out.x = icm->gyrX() * (PI / 180.0f);
    out.y = icm->gyrY() * (PI / 180.0f);
    out.z = icm->gyrZ() * (PI / 180.0f);
    return true;
}

#endif // ICM_20948 available
