/*
 * Fall Detection Module Implementation
 * Automatic fall detection and SOS triggering for unconscious users
 *
 * IMU-agnostic: uses FallSensorInterface abstraction.
 * Sensor adapter is injected via constructor (MPU6050, ICM20948, QMI8658).
 *
 * Gyro calibration: each boot measures the gyro bias and noise while the unit lies
 * still (specs/fall-detection REQ-FALL-CAL-01); free-fall and impact thresholds are fixed.
 */

#include "fall/FallDetectionModule.h"

#if TREKLINK_HAS_FALL_DETECTION

#include "alarm/BuzzerManager.h"
#include "fall/FallDatalog.h"
#include "sos/TrekLinkSOSHelper.h"
#include "Capabilities.h"
#include "main.h"

FallDetectionModule *fallDetectionModule;

// F12: Static LUT definition (declared in header)

FallDetectionModule::FallDetectionModule(FallSensorInterface *sensor)
    : SinglePortModule("FallDetection", meshtastic_PortNum_PRIVATE_APP), OSThread("FallDetection"), currentState(MONITORING),
      sensor(sensor), sensorInitialized(false), prealarmStartTime(0),
      lastAlarmBeepTime(0), isBeepOn(false)
{
    if (sensor) {
        LOG_INFO("FallDetection: Module created with %s sensor, will init in runOnce()", sensor->sensorName());
    } else {
        LOG_WARN("FallDetection: Module created with no sensor (fall detection disabled)");
    }
}

FallDetectionModule::~FallDetectionModule()
{
    delete sensor;
}

void FallDetectionModule::transitionToState(FallState newState)
{
    if (currentState == newState)
        return;

    LOG_DEBUG("FallDetection: State transition %d -> %d", currentState, newState);
#if TREKLINK_IMU_STREAM
    char detail[8];
    snprintf(detail, sizeof(detail), "%d", (int)newState);
    TREKLINK_IMU_EVENT("STATE", detail);
#endif
    currentState = newState;
}

float FallDetectionModule::calculateTotalAcceleration(const SensorVec3 &accel)
{
    return sqrt(sq(accel.x) + sq(accel.y) + sq(accel.z)) / 9.81f;
}

float FallDetectionModule::calculateTotalGyro(const SensorVec3 &gyro)
{
    return sqrt(sq(gyro.x - gyroBias[0]) + sq(gyro.y - gyroBias[1]) + sq(gyro.z - gyroBias[2]));
}

void FallDetectionModule::cancelFallAlarm()
{
    if (currentState == PRE_ALARM) {
        LOG_INFO("FallDetection: Fall alarm cancelled by user");
        deactivatePreAlarm();
        transitionToState(MONITORING);
    }
}

void FallDetectionModule::cancelAutoSOS()
{
    if (currentState == SOS_TRIGGERED) {
        LOG_INFO("FallDetection: Auto-SOS cancelled by user");
        TrekLinkSOSHelper::instance().deactivateAlarms(OWNER_FALL);
        transitionToState(MONITORING);
    }
}

void FallDetectionModule::activatePreAlarm()
{
    LOG_WARN("FallDetection: PRE-ALARM activated - 30s countdown");
    // The pre-alarm keeps its own 1 s beep (specs/sos REQ-SOS-ALM-03); the SOS pattern starts only at auto-SOS.
#ifdef PIN_BUZZER
    BuzzerManager::instance().acquire(OWNER_FALL);
#endif
    isBeepOn = true;
}

void FallDetectionModule::deactivatePreAlarm()
{
    LOG_INFO("FallDetection: Pre-alarm deactivated");
    TrekLinkSOSHelper::instance().deactivateAlarms(OWNER_FALL);
    isBeepOn = false;
}

void FallDetectionModule::updateAlarmFeedback()
{
    unsigned long elapsed = millis() - lastAlarmBeepTime;

    if (elapsed >= ALARM_BEEP_INTERVAL) {
        lastAlarmBeepTime = millis();
        isBeepOn = true;

#ifdef PIN_BUZZER
        BuzzerManager::instance().mark(128); // attach again after any stock tone (F2)
#endif
#ifdef PIN_VIBRATOR
        digitalWrite(PIN_VIBRATOR, HIGH);
#endif
    } else if (isBeepOn && elapsed >= 200) {
        isBeepOn = false;

#ifdef PIN_BUZZER
        BuzzerManager::instance().write(0);
#endif
#ifdef PIN_VIBRATOR
        digitalWrite(PIN_VIBRATOR, LOW);
#endif
    }
}

void FallDetectionModule::triggerAutoSOS()
{
    LOG_CRIT("FallDetection: AUTO-SOS TRIGGERED!");

    TrekLinkSOSHelper::instance().broadcastPosition();
    TrekLinkSOSHelper::instance().sendSOSTextMessage(TREKLINK_SOS_FALL_TAG);

    TrekLinkSOSHelper::instance().activateAlarms(OWNER_FALL); // the common SOS pattern (REQ-SOS-ALM-01)
}

// ----------------------------------------------------------------------
// Gyro calibration (REQ-FALL-CAL-01, findings F6 and F15)
// ----------------------------------------------------------------------

void FallDetectionModule::loadCalibration()
{
    // Read-write: stored values of another format version, and the keys the old self-calibration wrote
    // (ff_thresh in mixed units, F6), are erased.
    Preferences prefs;
    prefs.begin("fallcal", false);
    if (prefs.getUInt("ver", 0) == CAL_VERSION) {
        gyroBias[0] = prefs.getFloat("bx", 0);
        gyroBias[1] = prefs.getFloat("by", 0);
        gyroBias[2] = prefs.getFloat("bz", 0);
        gyroStillnessThreshold = prefs.getFloat("gstill", calibration.limits().defaultStillRads);
        LOG_INFO("FallDetection: Stored calibration bias=(%.3f,%.3f,%.3f) still=%.3frad/s", gyroBias[0], gyroBias[1],
                 gyroBias[2], gyroStillnessThreshold);
    } else {
        if (prefs.isKey("ver") || prefs.isKey("ff_thresh") || prefs.isKey("gyro_still")) {
            prefs.clear();
            LOG_INFO("FallDetection: Erased stored calibration of an old format");
        }
        gyroStillnessThreshold = calibration.limits().defaultStillRads;
    }
    prefs.end();
    detector.setStillGyro(gyroStillnessThreshold);
}

void FallDetectionModule::saveCalibration()
{
    Preferences prefs;
    prefs.begin("fallcal", false);
    prefs.putFloat("bx", gyroBias[0]);
    prefs.putFloat("by", gyroBias[1]);
    prefs.putFloat("bz", gyroBias[2]);
    prefs.putFloat("gstill", gyroStillnessThreshold);
    prefs.putUInt("ver", CAL_VERSION);
    prefs.end();
}

void FallDetectionModule::startCalibration(unsigned long now)
{
    calibration.restart();
    calRunning = true;
    lastCalStart = now;
}

void FallDetectionModule::feedCalibration(const SensorVec3 &accel, const SensorVec3 &gyro)
{
    const float g = 9.80665f;
    float a[3] = {accel.x / g, accel.y / g, accel.z / g};
    float w[3] = {gyro.x, gyro.y, gyro.z};
    treklink::fall::CalVerdict v = calibration.add(a, w);
    if (v == treklink::fall::CalVerdict::COLLECTING)
        return;

    calRunning = false;
    bootCalDone = true;
    const treklink::fall::CalibrationResult &r = calibration.result();
    const char *why = v == treklink::fall::CalVerdict::REJECTED_MOVING        ? "moving"
                      : v == treklink::fall::CalVerdict::REJECTED_MEAN_NOT_1G ? "mean not 1 g"
                      : v == treklink::fall::CalVerdict::REJECTED_GYRO_NOISE  ? "gyro noise"
                                                                              : "accepted";
    if (v == treklink::fall::CalVerdict::ACCEPTED) {
        for (int i = 0; i < 3; i++)
            gyroBias[i] = r.bias[i];
        gyroStillnessThreshold = r.stillRads;
        detector.setStillGyro(gyroStillnessThreshold);
        saveCalibration();
        calAccepted = true;
        LOG_INFO("FallDetection: Calibration accepted, bias=(%.3f,%.3f,%.3f) still=%.3frad/s", gyroBias[0], gyroBias[1],
                 gyroBias[2], gyroStillnessThreshold);
    } else {
        LOG_INFO("FallDetection: Calibration rejected (%s: mean %.3fg, accel std %.3fg, gyro std %.3frad/s), still=%.3frad/s, "
                 "retry in %lus",
                 why, r.meanG, r.accelStdG, r.gyroStdRads, gyroStillnessThreshold, CAL_RETRY_MS / 1000UL);
    }
#if TREKLINK_IMU_STREAM
    char detail[96];
    snprintf(detail, sizeof(detail), "%s mean=%.3f astd=%.3f gstd=%.3f still=%.3f", why, r.meanG, r.accelStdG, r.gyroStdRads,
             gyroStillnessThreshold);
    TREKLINK_IMU_EVENT("CALIBRATION", detail);
#endif
}

// ----------------------------------------------------------------------
// Main run loop
// ----------------------------------------------------------------------

int32_t FallDetectionModule::runOnce()
{
    if (!sensor) {
        return disable();
    }

    // F5: Lazy init
    if (!sensorInitialized) {
        if (!sensor->init()) {
            LOG_WARN("FallDetection: %s not ready, retrying in 5s", sensor->sensorName());
            return 5000;
        }

        sensorInitialized = true;
        LOG_INFO("FallDetection: %s initialized", sensor->sensorName());
        TREKLINK_IMU_BEGIN(sensor->sensorName());

#ifdef PIN_VIBRATOR
        pinMode(PIN_VIBRATOR, OUTPUT);
        digitalWrite(PIN_VIBRATOR, LOW);
#endif

        loadCalibration();
        startCalibration(millis());
        LOG_INFO("FallDetection: Calibrating the gyro (5 s, keep the unit still)");
        return SAMPLE_PERIOD_MS;
    }

    // F14: In SOS_TRIGGERED, skip I2C reads
    if (currentState == SOS_TRIGGERED) {
        // AlarmPlayer plays the SOS pattern on the buzzer, motor and LED.

        return 200;
    }

    // Read sensor data
    SensorVec3 accel, gyro;
    if (!sensor->readAccel(accel) || !sensor->readGyro(gyro)) {
        LOG_WARN("FallDetection: Sensor read failed");
        return 500;
    }

    float totalAccel = calculateTotalAcceleration(accel);
    float totalGyro = calculateTotalGyro(gyro);
    unsigned long now = millis();
    // Datalog state: 255 during the boot calibration, detector phase 0 to 3 while monitoring, then 4
    // pre-alarm, 5 SOS.
    TREKLINK_IMU_SAMPLE(accel, gyro, totalAccel, totalGyro,
                        !bootCalDone                 ? (uint8_t)255
                        : currentState == MONITORING ? (uint8_t)detector.phase()
                                                     : (uint8_t)(currentState + 3));

    if (calRunning)
        feedCalibration(accel, gyro);
    if (!bootCalDone)
        return SAMPLE_PERIOD_MS; // detection starts after the boot calibration's verdict
    if (!calAccepted && !calRunning && currentState == MONITORING && now - lastCalStart >= CAL_RETRY_MS)
        startCalibration(now);

    switch (currentState) {
    case MONITORING:
        switch (detector.step((uint32_t)now, totalAccel, totalGyro)) {
        case treklink::fall::Event::CHECK_STARTED:
            LOG_WARN("FallDetection: Fall check started (%s, free fall %ums, %.2fg)",
                     detector.cause() == treklink::fall::Cause::IMPACT ? "impact" : "free fall", (unsigned)detector.lastFreeFallMs(),
                     totalAccel);
            TREKLINK_IMU_EVENT("CHECK_STARTED", detector.cause() == treklink::fall::Cause::IMPACT ? "impact" : "free fall");
            break;
        case treklink::fall::Event::USER_OK:
            LOG_INFO("FallDetection: Movement after the fall, user is OK (peak %.2fg)", detector.peakG());
            TREKLINK_IMU_EVENT("USER_OK", nullptr);
            break;
        case treklink::fall::Event::RAISE_PREALARM:
            LOG_WARN("FallDetection: Still for %lus after the fall (peak %.2fg)", detector.config().stillMs / 1000UL,
                     detector.peakG());
            prealarmStartTime = now;
            lastAlarmBeepTime = now;
            transitionToState(PRE_ALARM);
            activatePreAlarm();
            break;
        default:
            break;
        }
        break;

    case PRE_ALARM:
        updateAlarmFeedback();
        if ((now - prealarmStartTime) >= PREALARM_TIMEOUT) {
            transitionToState(SOS_TRIGGERED);
            triggerAutoSOS();
        }
        break;

    default:
        break;
    }

    return SAMPLE_PERIOD_MS;
}

#else

FallDetectionModule *fallDetectionModule = nullptr;

#endif // guard
