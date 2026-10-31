// Unit tests for the fall sensor unit conversions (src/modules/sensors/FallSensorUnits.h).
//
// The v4 adapter (QMI8658FallSensor) cannot be built natively: it needs SensorLib, Wire and the
// TREKLINK_V4 guard. These tests feed the values SensorLib 0.3.4 produces into the same conversion
// functions the adapter uses. The fake readings model SensorQMI8658::getAccelerometer and
// getGyroscope (SensorQMI8658.hpp:910-923 and 975-988): raw LSB times the range scale.
//
// Runs under PlatformIO (`pio test -e native -f test_fall_sensor_units`) and on a bare host compiler;
// see the note at the bottom of this file.

#include "modules/sensors/FallSensorUnits.h"

#include <stdint.h>
#include <stdlib.h>
#include <unity.h>

using namespace treklink::fall;

// SensorLib 0.3.4 scales for the ranges QMI8658FallSensor::init configures.
static const float QMI_ACCEL_SCALE_8G = 8.0f / 32768.0f;      // SensorQMI8658.hpp:521, g per LSB
static const float QMI_GYRO_SCALE_512DPS = 512.0f / 32768.0f; // SensorQMI8658.hpp:598, dps per LSB

// Equivalent of SensorQMI8658::getAccelerometer for one axis, in g.
static float fakeQmiAccelG(int16_t raw)
{
    return raw * QMI_ACCEL_SCALE_8G;
}

// Equivalent of SensorQMI8658::getGyroscope for one axis, in degrees per second.
static float fakeQmiGyroDps(int16_t raw)
{
    return raw * QMI_GYRO_SCALE_512DPS;
}

void setUp(void) {}
void tearDown(void) {}

static void test_one_g_is_standard_gravity()
{
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 9.80665f, gToMps2(1.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -9.80665f, gToMps2(-1.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, gToMps2(0.0f));
}

// A v4 at rest, Z axis up: 4096 LSB at +-8 g is 1 g, which must reach the detector as about 9.8 m/s^2.
// The old adapter divided by 1000 and produced about 0.0098 m/s^2, which the detector reads as free fall.
static void test_qmi_at_rest_reads_about_one_g_in_mps2()
{
    float z = gToMps2(fakeQmiAccelG(4096));
    float x = gToMps2(fakeQmiAccelG(0));

    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 9.80665f, z);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, x);
    TEST_ASSERT_TRUE(z > 9.0f); // not the 0.0098 m/s^2 the mg assumption produced
}

// Full-scale +-8 g impact: 32767 LSB is just under 8 g.
static void test_qmi_full_scale_impact()
{
    float z = gToMps2(fakeQmiAccelG(32767));

    TEST_ASSERT_FLOAT_WITHIN(1e-2f, 8.0f * 9.80665f, z);
    TEST_ASSERT_FLOAT_WITHIN(1e-2f, -8.0f * 9.80665f, gToMps2(fakeQmiAccelG(-32768)));
}

static void test_dps_to_rad_s()
{
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 3.14159265f, dpsToRadS(180.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0174532925f, dpsToRadS(1.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -1.57079633f, dpsToRadS(-90.0f));
}

// 64 LSB at +-512 dps is 1 dps (SensorLib returns dps, so the adapter converts dps to rad/s).
static void test_qmi_gyro_reading_in_rad_s()
{
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, fakeQmiGyroDps(64));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0174532925f, dpsToRadS(fakeQmiGyroDps(64)));
    // 180 dps is 11520 LSB, which is pi rad/s.
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 3.14159265f, dpsToRadS(fakeQmiGyroDps(11520)));
}

static int runAll()
{
    UNITY_BEGIN();
    RUN_TEST(test_one_g_is_standard_gravity);
    RUN_TEST(test_qmi_at_rest_reads_about_one_g_in_mps2);
    RUN_TEST(test_qmi_full_scale_impact);
    RUN_TEST(test_dps_to_rad_s);
    RUN_TEST(test_qmi_gyro_reading_in_rad_s);
    return UNITY_END();
}

#ifdef TREKLINK_HOST_TEST
// Host build, no PlatformIO:
//   g++ -std=c++11 -DTREKLINK_HOST_TEST -Isrc -I<unity>/src test/test_fall_sensor_units/test_main.cpp
//       <unity>/src/unity.c
int main()
{
    return runAll();
}
#else
#include <Arduino.h> // declares setup() and loop() with the linkage portduino's main expects

void setup()
{
    exit(runAll());
}
void loop() {}
#endif
