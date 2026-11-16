#pragma once

#include "configuration.h"

#if defined(ARCH_PORTDUINO) && defined(TREKLINK_SCREENCAP) && !MESHTASTIC_EXCLUDE_I2C

#include "MotionSensor.h"

/**
 * TrekLink screen capture (tools/screencap): the v2's ICM-20948 stand-in. It
 * runs the same compass calibration steps as ICM20948Sensor (seed, window,
 * extrema, save) on a simulated magnetometer whose heading the capture script
 * sets, so the calibration screens come from the firmware's own drawing code.
 */
class ScreenCapMotionSensor : public MotionSensor
{
  public:
    explicit ScreenCapMotionSensor(ScanI2C::FoundDevice foundDevice) : MotionSensor(foundDevice) { instance = this; }

    bool init() override { return true; }
    int32_t runOnce() override;
    void calibrate(uint16_t forSeconds) override;

    /** The heading the simulated unit points to, in degrees. */
    void setHeading(float degrees) { heading = degrees; }

    static ScreenCapMotionSensor *instance;

  private:
    bool showingScreen = false;
    float heading = 0;
    float highestX = 0, lowestX = 0, highestY = 0, lowestY = 0, highestZ = 0, lowestZ = 0;
    static constexpr const char *compassCalibrationFileName = "/prefs/compass_icm20948.dat";
};

#endif
