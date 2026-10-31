#pragma once

#include "SinglePortModule.h"
#include "concurrency/OSThread.h"
#include "Capabilities.h"

#if TREKLINK_HAS_FALL_DETECTION

#include "fall/FallCalibration.h"
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


    // Gyro stillness threshold handed to the detector (set by the calibration)
    float gyroStillnessThreshold = 1.0f;                          // rad/s
    static constexpr int32_t SAMPLE_PERIOD_MS = 20;               // requested; durations use timestamps
    static constexpr unsigned long PREALARM_TIMEOUT = 30000;     // 30 seconds
    static constexpr unsigned long ALARM_BEEP_INTERVAL = 1000;   // 1 second beep interval


    // Gyro calibration (fall/FallCalibration.h, specs/fall-detection REQ-FALL-CAL-01). Each boot
    // calibrates while the unit lies still; the stored version-2 result, or the default, is the fallback.
    treklink::fall::FallCalibration calibration;
    bool calRunning = false;  // samples are going into the calibration
    bool bootCalDone = false; // the boot calibration gave its verdict; detection runs from then on
    bool calAccepted = false; // a calibration was accepted in this boot
    unsigned long lastCalStart = 0;
    float gyroBias[3] = {0, 0, 0}; // rad/s, removed before the stillness test (F15)
    static constexpr unsigned long CAL_RETRY_MS = 60000;
    static constexpr uint32_t CAL_VERSION = 2;

    void loadCalibration();
    void saveCalibration();
    void startCalibration(unsigned long now);
    void feedCalibration(const SensorVec3 &accel, const SensorVec3 &gyro);

    // Helper methods
    void transitionToState(FallState newState);
    float calculateTotalAcceleration(const SensorVec3 &accel);
    float calculateTotalGyro(const SensorVec3 &gyro); // bias-corrected magnitude

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
