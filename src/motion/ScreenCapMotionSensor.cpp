#include "ScreenCapMotionSensor.h"

#if defined(ARCH_PORTDUINO) && defined(TREKLINK_SCREENCAP) && !MESHTASTIC_EXCLUDE_I2C

#include <math.h>

ScreenCapMotionSensor *ScreenCapMotionSensor::instance = nullptr;

// A horizontal field of 30 uT pointing to magnetic north, seen from a unit
// turned to `heading`.
static void simulatedField(float heading, float &x, float &y, float &z)
{
    const float rad = heading * PI / 180;
    x = 30 * cosf(rad);
    y = -30 * sinf(rad);
    z = -40;
}

int32_t ScreenCapMotionSensor::runOnce()
{
    float magX, magY, magZ;
    simulatedField(heading, magX, magY, magZ);
    if (doCalibration) {
        beginCalibrationDisplay(showingScreen);
        updateCalibrationExtrema(magX, magY, magZ, highestX, lowestX, highestY, lowestY, highestZ, lowestZ);
        finishCalibrationIfExpired(showingScreen, compassCalibrationFileName, highestX, lowestX, highestY, lowestY, highestZ,
                                   lowestZ);
    }
    if (screen)
        screen->setHeading(applyCompassOrientation(heading));
    return MOTION_SENSOR_CHECK_INTERVAL_MS;
}

void ScreenCapMotionSensor::calibrate(uint16_t forSeconds)
{
#if !defined(MESHTASTIC_EXCLUDE_SCREEN) && HAS_SCREEN
    LOG_DEBUG("ScreenCap motion cal start %is", forSeconds);
    float x, y, z;
    simulatedField(heading, x, y, z);
    seedCalibrationExtrema(x, y, z, highestX, lowestX, highestY, lowestY, highestZ, lowestZ);
    startCalibrationWindow(forSeconds);
#endif
}

#endif
