/*
 * ICM20948FallSensor Implementation
 * Adapter for SparkFun ICM-20948 → FallSensorInterface
 */

#include "fall/sensors/ICM20948FallSensor.h"

#if defined(TREKLINK_FALL_SENSOR_ICM20948) && __has_include(<ICM_20948.h>)

#include "motion/ICM20948Sensor.h"
#include "Capabilities.h"
#include "main.h"

// Stock ICM20948Singleton keeps its instance in a private static and offers no "initialized" accessor;
// GetInstance() would create an unconnected instance whose first I2C call dereferences a null serif.
// The pointer is read through its linker symbol instead (specs/architecture design §4.2 O-11). Stock
// ICM20948Sensor::init() sets it through GetInstance() and calls ICM20948Singleton::init() right after,
// whose begin() links the I2C interface first, so a non-null pointer is safe to call on the
// cooperative loop. It stays set if init() failed later, so the WHO_AM_I check below is what decides
// whether to join.
extern ICM20948Singleton *treklinkStockIcmInstance asm("_ZN17ICM20948Singleton9pinstanceE");

bool ICM20948FallSensor::init()
{
    // When the stock motion driver owns the chip (with the compass), join it and skip a second reset.
    if (treklinkStockIcmInstance != nullptr && treklinkStockIcmInstance->getWhoAmI() == ICM_20948_WHOAMI) {
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

    if (fallbackIcm.setSampleMode((ICM_20948_Internal_Acc | ICM_20948_Internal_Gyr), ICM_20948_Sample_Mode_Continuous) !=
        ICM_20948_Stat_Ok) {
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
