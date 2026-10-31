// Native test of the gyro calibration core (specs/fall-detection REQ-FALL-CAL-01; findings F6, F15).
//   pio test -e native -f treklink_fall_cal
//   host: g++ -std=c++11 -I<unity>/src test/treklink_fall_cal/test_main.cpp <unity>/src/unity.c

#include "../../treklink/src/fall/FallCalibration.h"

#include <cmath>
#include <unity.h>

using namespace treklink::fall;

void setUp() {}
void tearDown() {}

// Deterministic noise in [-amp, amp].
static float noise(uint32_t &seed, float amp)
{
    seed = seed * 1664525u + 1013904223u;
    return ((seed >> 8) / 8388608.0f - 1.0f) * amp;
}

static CalVerdict feed(FallCalibration &c, float az, float accelNoise, const float bias[3], float gyroNoise,
                       float swing = 0)
{
    uint32_t seed = 7;
    CalVerdict v = CalVerdict::COLLECTING;
    for (int i = 0; v == CalVerdict::COLLECTING; i++) {
        float a[3] = {noise(seed, accelNoise) + (i % 2 ? swing : -swing), noise(seed, accelNoise), az + noise(seed, accelNoise)};
        float g[3] = {bias[0] + noise(seed, gyroNoise), bias[1] + noise(seed, gyroNoise), bias[2] + noise(seed, gyroNoise)};
        v = c.add(a, g);
    }
    return v;
}

static void test_still_level_run_is_accepted_and_removes_the_bias_F15()
{
    FallCalibration c;
    const float bias[3] = {0.75f, -0.02f, 0.01f}; // the resting bias an earlier v2 prototype recorded
    TEST_ASSERT_EQUAL((int)CalVerdict::ACCEPTED, (int)feed(c, 1.0f, 0.01f, bias, 0.02f));
    TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.75f, c.result().bias[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.005f, -0.02f, c.result().bias[1]);
    // uniform noise of +-0.02 has std 0.0115; 3x is about 0.035, clamped up to 0.05
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.05f, c.result().stillRads);
}

static void test_moving_run_is_rejected_F6()
{
    FallCalibration c;
    const float bias[3] = {0, 0, 0};
    TEST_ASSERT_EQUAL((int)CalVerdict::REJECTED_MOVING, (int)feed(c, 1.0f, 0.01f, bias, 0.02f, 0.3f));
}

static void test_mean_away_from_1g_is_rejected()
{
    FallCalibration c;
    const float bias[3] = {0, 0, 0};
    TEST_ASSERT_EQUAL((int)CalVerdict::REJECTED_MEAN_NOT_1G, (int)feed(c, 0.8f, 0.01f, bias, 0.02f));
}

static void test_noisy_gyro_is_rejected()
{
    FallCalibration c;
    const float bias[3] = {0, 0, 0};
    TEST_ASSERT_EQUAL((int)CalVerdict::REJECTED_GYRO_NOISE, (int)feed(c, 1.0f, 0.01f, bias, 0.3f));
}

static void test_threshold_is_clamped_to_the_maximum()
{
    CalibrationLimits lim;
    lim.maxGyroStdRads = 1.0f; // let a noisy gyro through to reach the clamp
    FallCalibration c(lim);
    const float bias[3] = {0, 0, 0};
    TEST_ASSERT_EQUAL((int)CalVerdict::ACCEPTED, (int)feed(c, 1.0f, 0.01f, bias, 0.3f));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.3f, c.result().stillRads);
}

static void test_restart_discards_earlier_samples()
{
    FallCalibration c;
    const float bias[3] = {0, 0, 0};
    float a[3] = {0.5f, 0, 0.2f}, g[3] = {1, 1, 1};
    for (int i = 0; i < 100; i++)
        c.add(a, g);
    c.restart();
    TEST_ASSERT_EQUAL((int)CalVerdict::ACCEPTED, (int)feed(c, 1.0f, 0.01f, bias, 0.02f));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_still_level_run_is_accepted_and_removes_the_bias_F15);
    RUN_TEST(test_moving_run_is_rejected_F6);
    RUN_TEST(test_mean_away_from_1g_is_rejected);
    RUN_TEST(test_noisy_gyro_is_rejected);
    RUN_TEST(test_threshold_is_clamped_to_the_maximum);
    RUN_TEST(test_restart_discards_earlier_samples);
    return UNITY_END();
}
