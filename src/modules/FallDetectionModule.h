#pragma once

#include "SinglePortModule.h"
#include "concurrency/OSThread.h"
#include "configuration.h"

#if !defined(ARCH_STM32WL) && !MESHTASTIC_EXCLUDE_I2C && defined(TREKLINK_VARIANT) && !defined(TREKLINK_V3)

#include "FallSensorInterface.h"
#include <Preferences.h>

// TrekLink message type discriminator (F8 - shared with TrekLinkSOSHelper.h)
#ifndef TREKLINK_MSG_FALL
#define TREKLINK_MSG_FALL 0x02
#endif

/**
 * Fall Detection Module for TrekLink
 * Monitors IMU accelerometer/gyroscope for fall patterns via FallSensorInterface.
 * Triggers automatic SOS if user becomes unconscious after fall.
 *
 * IMU-agnostic: accepts any FallSensorInterface adapter (MPU6050, ICM20948, QMI8658).
 *
 * Features:
 * - Self-calibration: measures per-chip noise floor at boot and scales thresholds
 * - Stationary persistence: calibrated thresholds survive reboot via NVS
 */

class FallDetectionModule : public SinglePortModule, public concurrency::OSThread
{
  public:
    explicit FallDetectionModule(FallSensorInterface *sensor);
    ~FallDetectionModule();

    void cancelFallAlarm();
    void cancelAutoSOS();
    bool isInPreAlarm() const { return currentState == PRE_ALARM; }
    bool isInSOSTriggered() const { return currentState == SOS_TRIGGERED; }

  protected:
    virtual int32_t runOnce() override;

  private:
    // Fall detection state machine (IMPACT_DETECTED removed — falls validated via 10s inactivity)
    enum FallState {
        MONITORING,          // Normal operation, watching for falls
        FREEFALL_DETECTED,   // Detected freefall condition (<threshold g)
        INACTIVITY_DETECTED, // No movement detected after fall
        PRE_ALARM,           // Countdown active, waiting for user cancel
        SOS_TRIGGERED        // Auto-SOS sent, alarm active (F21: has exit path)
    };

    FallState currentState;
    FallSensorInterface *sensor;
    bool sensorInitialized;

    // Timing trackers
    unsigned long freefallStartTime;
    unsigned long inactivityStartTime;
    unsigned long prealarmStartTime;
    unsigned long lastAlarmBeepTime;
    bool isBeepOn;

    // SOS Morse code pattern state (F12: LUT-based)
    unsigned long sosPatternStartTime;
    bool sosBuzzerOn;

    // Detection thresholds (tunable by self-calibration)
    float freefallThreshold = 0.5f;          // g (<threshold indicates freefall)
    float gyroStillnessThreshold = 0.1f;     // rad/s (gyro movement threshold)
    static constexpr unsigned long FREEFALL_MIN_DURATION = 500;  // ms
    static constexpr unsigned long INACTIVITY_DURATION = 10000;  // 10 seconds
    static constexpr unsigned long PREALARM_TIMEOUT = 30000;     // 30 seconds
    static constexpr unsigned long ALARM_BEEP_INTERVAL = 1000;   // 1 second beep interval

    // F12: SOS Morse LUT
    static constexpr bool SOS_LUT[28] = {
        1,0,1,0,1,0,                       // S (...)
        1,1,1,0,1,1,1,0,1,1,1,0,           // O (---)
        1,0,1,0,1,0,                       // S (...)
        0,0,0,0                             // Gap
    };
    static constexpr uint8_t SOS_LUT_LEN = 28;

    // Phase 2: Self-calibration state
    enum CalPhase { CAL_NONE, CAL_COLLECTING, CAL_COMPLETE };
    CalPhase calPhase = CAL_NONE;

    static constexpr int CAL_SAMPLES = 250;  // 5s @ 50Hz
    static constexpr int CAL_RATE_MS = 20;   // 50Hz sampling during calibration
    int calSampleCount = 0;

    struct CalStats {
        float sum_x, sum_y, sum_z;
        float sum_xx, sum_yy, sum_zz;
        float mean_x, mean_y, mean_z;
        float std_x, std_y, std_z;
    };
    CalStats accelStats, gyroStats;

    float calibratedFreefallThreshold = 0.5f;
    float calibratedGyroStillness = 0.1f;

    void collectCalibrationSample(const SensorVec3 &accel, const SensorVec3 &gyro);
    void finalizeCalibration();
    void loadCalibration();
    void saveCalibration();

    // Helper methods
    void transitionToState(FallState newState);
    float calculateTotalAcceleration(const SensorVec3 &accel);
    float calculateTotalGyro(const SensorVec3 &gyro);
    bool checkInactivity(const SensorVec3 &accel, const SensorVec3 &gyro);

    void activatePreAlarm();
    void deactivatePreAlarm();
    void triggerAutoSOS();
    void updateAlarmFeedback();
    void updateSOSBuzzerPattern();
};

extern FallDetectionModule *fallDetectionModule;

#else // stub for non-TrekLink or TREKLINK_V3 builds

class FallDetectionModule {
  public:
    void cancelFallAlarm() {}
    void cancelAutoSOS() {}
    bool isInPreAlarm() const { return false; }
    bool isInSOSTriggered() const { return false; }
};

extern FallDetectionModule *fallDetectionModule;

#endif // guard
