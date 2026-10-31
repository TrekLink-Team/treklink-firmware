/*
 * Fall Detection Module Implementation
 * Automatic fall detection and SOS triggering for unconscious users
 *
 * IMU-agnostic: uses FallSensorInterface abstraction.
 * Sensor adapter is injected via constructor (MPU6050, ICM20948, QMI8658).
 *
 * Self-calibration: noise floor measured at first boot, thresholds adjusted
 * per-chip and persisted in NVS.
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
    return sqrt(sq(gyro.x) + sq(gyro.y) + sq(gyro.z));
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
// Phase 2: Self-calibration
// ----------------------------------------------------------------------

void FallDetectionModule::collectCalibrationSample(const SensorVec3 &accel, const SensorVec3 &gyro)
{
    accelStats.sum_x += accel.x;
    accelStats.sum_y += accel.y;
    accelStats.sum_z += accel.z;
    accelStats.sum_xx += accel.x * accel.x;
    accelStats.sum_yy += accel.y * accel.y;
    accelStats.sum_zz += accel.z * accel.z;

    gyroStats.sum_x += gyro.x;
    gyroStats.sum_y += gyro.y;
    gyroStats.sum_z += gyro.z;
    gyroStats.sum_xx += gyro.x * gyro.x;
    gyroStats.sum_yy += gyro.y * gyro.y;
    gyroStats.sum_zz += gyro.z * gyro.z;
}

void FallDetectionModule::finalizeCalibration()
{
    float n = (float)CAL_SAMPLES;

    accelStats.mean_x = accelStats.sum_x / n;
    accelStats.mean_y = accelStats.sum_y / n;
    accelStats.mean_z = accelStats.sum_z / n;
    accelStats.std_x = sqrt(accelStats.sum_xx / n - accelStats.mean_x * accelStats.mean_x);
    accelStats.std_y = sqrt(accelStats.sum_yy / n - accelStats.mean_y * accelStats.mean_y);
    accelStats.std_z = sqrt(accelStats.sum_zz / n - accelStats.mean_z * accelStats.mean_z);

    gyroStats.mean_x = gyroStats.sum_x / n;
    gyroStats.mean_y = gyroStats.sum_y / n;
    gyroStats.mean_z = gyroStats.sum_z / n;
    gyroStats.std_x = sqrt(gyroStats.sum_xx / n - gyroStats.mean_x * gyroStats.mean_x);
    gyroStats.std_y = sqrt(gyroStats.sum_yy / n - gyroStats.mean_y * gyroStats.mean_y);
    gyroStats.std_z = sqrt(gyroStats.sum_zz / n - gyroStats.mean_z * gyroStats.mean_z);

    float accelNoise = (accelStats.std_x + accelStats.std_y + accelStats.std_z) / 3.0f;
    float gyroNoise = (gyroStats.std_x + gyroStats.std_y + gyroStats.std_z) / 3.0f;

    calibratedFreefallThreshold = fmin(1.0f, fmax(0.3f, 3.5f * accelNoise));
    calibratedGyroStillness = fmin(0.3f, fmax(0.05f, 3.0f * gyroNoise));

    LOG_INFO("FallDetection: Calibration complete — accelNoise=%.4fg, freefall=%.2fg, gyroStill=%.3frad/s", accelNoise,
             calibratedFreefallThreshold, calibratedGyroStillness);
}

void FallDetectionModule::saveCalibration()
{
    Preferences prefs;
    prefs.begin("fallcal", false);
    prefs.putFloat("ff_thresh", calibratedFreefallThreshold);
    prefs.putFloat("gyro_still", calibratedGyroStillness);
    prefs.end();
}

void FallDetectionModule::loadCalibration()
{
    Preferences prefs;
    prefs.begin("fallcal", true);
    if (prefs.isKey("ff_thresh")) {
        calibratedFreefallThreshold = prefs.getFloat("ff_thresh", 0.5f);
        calibratedGyroStillness = prefs.getFloat("gyro_still", 0.1f);
        calPhase = CAL_COMPLETE;
        LOG_INFO("FallDetection: Loaded stored calibration (ff=%.2f, gyro=%.3f)", calibratedFreefallThreshold,
                 calibratedGyroStillness);
    }
    prefs.end();
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

        // Enter self-calibration phase
        calPhase = CAL_COLLECTING;
        calSampleCount = 0;
        memset(&accelStats, 0, sizeof(accelStats));
        memset(&gyroStats, 0, sizeof(gyroStats));

        loadCalibration();
        if (calPhase == CAL_COMPLETE) {
            // Use the stored gyro stillness threshold; free-fall and impact thresholds are fixed (REQ-FALL-CAL-01)
            gyroStillnessThreshold = calibratedGyroStillness;
            detector.setStillGyro(gyroStillnessThreshold);
            LOG_INFO("FallDetection: Using stored calibration, entering MONITORING");
            return 100;
        }

        LOG_INFO("FallDetection: Starting self-calibration (5s stationary at 50Hz)...");
        return CAL_RATE_MS;
    }

    // Phase 2: Calibration data collection
    if (calPhase == CAL_COLLECTING) {
        SensorVec3 calAccel, calGyro;
        sensor->readAccel(calAccel);
        sensor->readGyro(calGyro);
        TREKLINK_IMU_SAMPLE(calAccel, calGyro, calculateTotalAcceleration(calAccel), calculateTotalGyro(calGyro), 255);
        collectCalibrationSample(calAccel, calGyro);
        calSampleCount++;

        if (calSampleCount >= CAL_SAMPLES) {
            finalizeCalibration();
#if TREKLINK_IMU_STREAM
            char detail[48];
            snprintf(detail, sizeof(detail), "ff=%.3f gyro=%.3f", calibratedFreefallThreshold, calibratedGyroStillness);
            TREKLINK_IMU_EVENT("CALIBRATED", detail);
#endif
            saveCalibration();
            calPhase = CAL_COMPLETE;

            // Apply the calibrated gyro stillness threshold; free-fall and impact thresholds are fixed
            gyroStillnessThreshold = calibratedGyroStillness;
            detector.setStillGyro(gyroStillnessThreshold);

            LOG_INFO("FallDetection: Calibration applied, entering MONITORING");
        }

        return CAL_RATE_MS;
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
    // Datalog state: detector phase 0 to 3 while monitoring, then 4 pre-alarm, 5 SOS.
    TREKLINK_IMU_SAMPLE(accel, gyro, totalAccel, totalGyro,
                        currentState == MONITORING ? (uint8_t)detector.phase() : (uint8_t)(currentState + 3));
    unsigned long now = millis();

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
