#pragma once

#include "SinglePortModule.h"
#include "concurrency/OSThread.h"
#include "Capabilities.h"

#if TREKLINK_HAS_FALL_DETECTION

#include "fall/FallDetector.h"
#include "fall/FallSensorInterface.h"
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
    // Module states. Free fall, impact, settle and the stillness window live in the platform-free
    // detector core (fall/FallDetector.h, specs/fall-detection design §2).
    enum FallState {
        MONITORING,   // the detector runs on every sample
        PRE_ALARM,    // countdown active, waiting for user cancel
        SOS_TRIGGERED // auto-SOS sent, alarm active (F21: has exit path)
    };

    FallState currentState;
    FallSensorInterface *sensor;
    bool sensorInitialized;

    treklink::fall::FallDetector detector;

    // Timing trackers
    unsigned long prealarmStartTime;
    unsigned long lastAlarmBeepTime;
    bool isBeepOn;


    // Gyro stillness threshold handed to the detector (tunable by self-calibration)
    float gyroStillnessThreshold = 0.1f;                          // rad/s
    static constexpr int32_t SAMPLE_PERIOD_MS = 20;               // requested; durations use timestamps
    static constexpr unsigned long PREALARM_TIMEOUT = 30000;     // 30 seconds
    static constexpr unsigned long ALARM_BEEP_INTERVAL = 1000;   // 1 second beep interval


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

    void activatePreAlarm();
    void deactivatePreAlarm();
    void triggerAutoSOS();
    void updateAlarmFeedback();
};

extern FallDetectionModule *fallDetectionModule;

#else // stub for variants without fall detection

class FallDetectionModule {
  public:
    void cancelFallAlarm() {}
    void cancelAutoSOS() {}
    bool isInPreAlarm() const { return false; }
    bool isInSOSTriggered() const { return false; }
};

extern FallDetectionModule *fallDetectionModule;

#endif // guard
