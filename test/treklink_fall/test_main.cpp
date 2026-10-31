// Native test of the fall detector core (specs/fall-detection REQ-FALL-SIG-01, REQ-FALL-STL-01).
//   pio test -e native -f treklink_fall
//   host: g++ -std=c++11 -I<unity>/src test/treklink_fall/test_main.cpp <unity>/src/unity.c

#include "../../treklink/src/fall/FallDetector.h"

#include <unity.h>
#include <vector>

using namespace treklink::fall;

void setUp() {}
void tearDown() {}

struct Sample {
    uint32_t t;
    float a;
    float w;
};

// A recording: a run of samples at a fixed period, built phase by phase.
struct Trace {
    std::vector<Sample> s;
    uint32_t t = 0;
    uint32_t period;
    explicit Trace(uint32_t periodMs = 20) : period(periodMs) {}
    Trace &hold(uint32_t ms, float a, float w)
    {
        for (uint32_t end = t + ms; t < end; t += period)
            s.push_back({t, a, w});
        return *this;
    }
    Trace &walk(uint32_t ms) // moving: a swings around 1 g, gyro above the stillness threshold
    {
        for (uint32_t end = t + ms; t < end; t += period)
            s.push_back({t, (t / period) % 2 ? 1.4f : 0.7f, 2.0f});
        return *this;
    }
};

struct Result {
    int checks = 0, ok = 0, prealarm = 0;
    uint32_t prealarmAt = 0;
    Cause cause = Cause::NONE;
};

static Result run(const Trace &tr, DetectorConfig cfg = DetectorConfig())
{
    FallDetector d(cfg);
    Result r;
    for (const Sample &x : tr.s) {
        Event e = d.step(x.t, x.a, x.w);
        if (e == Event::CHECK_STARTED) {
            r.checks++;
            r.cause = d.cause();
        } else if (e == Event::USER_OK) {
            r.ok++;
        } else if (e == Event::RAISE_PREALARM) {
            r.prealarm++;
            r.prealarmAt = x.t;
        }
    }
    return r;
}

// The rule replaced by REQ-FALL-STL-01 (FallDetectionModule.cpp:337-363 at 80ccbe08a): stillness counted
// from the sample that ends free fall, the first sample that is not still cancels. Kept here so the
// test shows the defect F4 it fixes.
static bool oldRuleRaises(const Trace &tr)
{
    enum { MON, FF, INACT } st = MON;
    uint32_t ffStart = 0, inStart = 0;
    for (const Sample &x : tr.s) {
        bool still = (x.a > 0.8f && x.a < 1.2f) && x.w < 1.0f;
        if (st == MON && x.a < 0.5f) {
            st = FF;
            ffStart = x.t;
        } else if (st == FF && x.a >= 0.5f) {
            if (x.t - ffStart >= 500) {
                st = INACT;
                inStart = x.t;
            } else {
                st = MON;
            }
        }
        if (st == INACT) {
            if (!still)
                st = MON;
            else if (x.t - inStart >= 10000)
                return true;
        }
    }
    return false;
}

// Fall from a height: 0.6 s free fall, impact with bounce, then lying still.
static Trace fallWithBounce()
{
    Trace tr;
    tr.hold(2000, 1.0f, 0.1f).hold(600, 0.1f, 1.5f).hold(40, 6.0f, 4.0f).hold(300, 1.8f, 3.0f).hold(200, 0.6f, 2.5f);
    tr.hold(15000, 1.0f, 0.1f);
    return tr;
}

static void test_old_rule_misses_a_fall_with_bounce_F4()
{
    TEST_ASSERT_FALSE(oldRuleRaises(fallWithBounce()));
}

static void test_fall_with_bounce_raises_prealarm()
{
    Result r = run(fallWithBounce());
    TEST_ASSERT_EQUAL(1, r.checks);
    TEST_ASSERT_EQUAL(1, r.prealarm);
    TEST_ASSERT_EQUAL(0, r.ok);
    // impact pair ends at 2620 ms; settle 2 s; then 10 s still -> about 14.6 s
    TEST_ASSERT_UINT32_WITHIN(200, 14620, r.prealarmAt);
}

static void test_free_fall_without_impact_starts_check()
{
    Trace tr;
    tr.hold(1000, 1.0f, 0.1f).hold(400, 0.2f, 0.5f).hold(14000, 1.0f, 0.1f);
    Result r = run(tr);
    TEST_ASSERT_EQUAL(1, r.checks);
    TEST_ASSERT_EQUAL((int)Cause::FREE_FALL, (int)r.cause);
    TEST_ASSERT_EQUAL(1, r.prealarm);
}

static void test_impact_without_free_fall_starts_check()
{
    Trace tr;
    tr.walk(3000).hold(60, 4.0f, 3.0f).hold(14000, 1.0f, 0.1f);
    Result r = run(tr);
    TEST_ASSERT_EQUAL(1, r.checks);
    TEST_ASSERT_EQUAL((int)Cause::IMPACT, (int)r.cause);
    TEST_ASSERT_EQUAL(1, r.prealarm);
}

static void test_single_impact_sample_is_ignored()
{
    Trace tr;
    tr.hold(1000, 1.0f, 0.1f).hold(20, 4.0f, 1.0f).hold(14000, 1.0f, 0.1f);
    TEST_ASSERT_EQUAL(0, run(tr).checks);
}

static void test_short_free_fall_is_ignored()
{
    Trace tr;
    tr.hold(1000, 1.0f, 0.1f).hold(200, 0.2f, 0.5f).hold(14000, 1.0f, 0.1f);
    Result r = run(tr);
    TEST_ASSERT_EQUAL(0, r.checks);
    TEST_ASSERT_EQUAL(0, r.prealarm);
}

static void test_a_move_restarts_the_still_run()
{
    Trace tr;
    tr.hold(500, 1.0f, 0.1f).hold(60, 5.0f, 3.0f).hold(2000, 1.0f, 0.1f); // impact, settle
    tr.hold(6000, 1.0f, 0.1f).walk(200).hold(10200, 1.0f, 0.1f);       // 6 s still, a twitch, 10 s still
    Result r = run(tr);
    TEST_ASSERT_EQUAL(1, r.prealarm);
    TEST_ASSERT_EQUAL(0, r.ok);
    // raised 10 s after the twitch, not 10 s after the settle
    TEST_ASSERT_UINT32_WITHIN(100, 2560 + 6000 + 200 + 10000, r.prealarmAt);
}

static void test_moving_after_the_fall_is_user_ok()
{
    Trace tr;
    tr.hold(500, 1.0f, 0.1f).hold(60, 5.0f, 3.0f).walk(30000);
    Result r = run(tr);
    TEST_ASSERT_EQUAL(1, r.checks);
    TEST_ASSERT_EQUAL(1, r.ok);
    TEST_ASSERT_EQUAL(0, r.prealarm);
}

static void test_still_run_starting_late_in_the_window_completes()
{
    Trace tr;
    tr.hold(500, 1.0f, 0.1f).hold(60, 5.0f, 3.0f).walk(2000 + 19000).hold(11000, 1.0f, 0.1f);
    Result r = run(tr);
    TEST_ASSERT_EQUAL(1, r.prealarm);
    TEST_ASSERT_EQUAL(0, r.ok);
}

static void test_still_run_starting_after_the_window_is_user_ok()
{
    Trace tr;
    tr.hold(500, 1.0f, 0.1f).hold(60, 5.0f, 3.0f).walk(2000 + 21000).hold(11000, 1.0f, 0.1f);
    Result r = run(tr);
    TEST_ASSERT_EQUAL(1, r.ok);
    TEST_ASSERT_EQUAL(0, r.prealarm);
}

static void test_jittered_period_measures_time_from_timestamps()
{
    // 27 ms real period (an earlier prototype measured this on v2); the result must not depend on it.
    Trace tr(27);
    tr.hold(2000, 1.0f, 0.1f).hold(600, 0.1f, 1.5f).hold(60, 6.0f, 4.0f).hold(15000, 1.0f, 0.1f);
    Result r = run(tr);
    TEST_ASSERT_EQUAL(1, r.prealarm);
    TEST_ASSERT_UINT32_WITHIN(100, 2000 + 600 + 54 + 2000 + 10000, r.prealarmAt);
}

static void test_gyro_bias_above_threshold_is_never_still_F15()
{
    // Uncorrected bias of 0.75 rad/s against a calibrated threshold of 0.3 rad/s: never still.
    DetectorConfig cfg;
    cfg.stillGyroRads = 0.3f;
    Trace tr;
    tr.hold(500, 1.0f, 0.75f).hold(60, 5.0f, 3.0f).hold(30000, 1.0f, 0.75f);
    Result r = run(tr, cfg);
    TEST_ASSERT_EQUAL(0, r.prealarm);
    TEST_ASSERT_EQUAL(1, r.ok);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_old_rule_misses_a_fall_with_bounce_F4);
    RUN_TEST(test_fall_with_bounce_raises_prealarm);
    RUN_TEST(test_free_fall_without_impact_starts_check);
    RUN_TEST(test_impact_without_free_fall_starts_check);
    RUN_TEST(test_single_impact_sample_is_ignored);
    RUN_TEST(test_short_free_fall_is_ignored);
    RUN_TEST(test_a_move_restarts_the_still_run);
    RUN_TEST(test_moving_after_the_fall_is_user_ok);
    RUN_TEST(test_still_run_starting_late_in_the_window_completes);
    RUN_TEST(test_still_run_starting_after_the_window_is_user_ok);
    RUN_TEST(test_jittered_period_measures_time_from_timestamps);
    RUN_TEST(test_gyro_bias_above_threshold_is_never_still_F15);
    return UNITY_END();
}
