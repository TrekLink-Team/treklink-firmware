// Native test of the common SOS pattern (specs/sos/requirements.md REQ-SOS-ALM-01).
//   pio test -e native -f treklink_alarm
//   host: g++ -std=c++11 -DTREKLINK_HOST_TEST -I<unity>/src test/treklink_alarm/test_main.cpp <unity>/src/unity.c

#include "../../treklink/src/alarm/SosPattern.h"

#include <unity.h>

using namespace treklink::alarm;

void setUp() {}
void tearDown() {}

// Count marks and their lengths in one period, sampled every millisecond.
static void test_one_period_is_morse_sos()
{
    int marks = 0, shortMarks = 0, longMarks = 0;
    uint32_t onFor = 0;
    for (uint32_t t = 0; t <= SOS_PERIOD_MS; t++) {
        bool on = t < SOS_PERIOD_MS && sosMarkAt(t);
        if (on) {
            onFor++;
        } else if (onFor) {
            marks++;
            if (onFor == SOS_UNIT_MS)
                shortMarks++;
            else if (onFor == 3 * SOS_UNIT_MS)
                longMarks++;
            onFor = 0;
        }
    }
    TEST_ASSERT_EQUAL(9, marks);
    TEST_ASSERT_EQUAL(6, shortMarks);
    TEST_ASSERT_EQUAL(3, longMarks);
}

static void test_order_is_short_long_short()
{
    // First mark short, the fourth long, the seventh short.
    TEST_ASSERT_TRUE(sosMarkAt(0));
    TEST_ASSERT_FALSE(sosMarkAt(SOS_UNIT_MS));
    TEST_ASSERT_TRUE(sosMarkAt(8 * SOS_UNIT_MS));
    TEST_ASSERT_TRUE(sosMarkAt(10 * SOS_UNIT_MS)); // still inside the first long mark
    TEST_ASSERT_TRUE(sosMarkAt(22 * SOS_UNIT_MS));
    TEST_ASSERT_FALSE(sosMarkAt(23 * SOS_UNIT_MS));
}

static void test_silent_after_the_pattern_and_repeats()
{
    for (uint32_t t = SOS_UNITS * SOS_UNIT_MS; t < SOS_PERIOD_MS; t++)
        TEST_ASSERT_FALSE(sosMarkAt(t));
    for (uint32_t t = 0; t < SOS_PERIOD_MS; t += 7)
        TEST_ASSERT_EQUAL(sosMarkAt(t), sosMarkAt(t + 3 * SOS_PERIOD_MS));
}

static void test_millis_wrap_does_not_break_the_period()
{
    // Callers pass now - start as uint32_t, which stays correct across a millis() wrap.
    uint32_t start = 0xFFFFFF00u;
    uint32_t now = start + 8 * SOS_UNIT_MS; // wraps
    TEST_ASSERT_TRUE(sosMarkAt(now - start));
}

static int runAll()
{
    UNITY_BEGIN();
    RUN_TEST(test_one_period_is_morse_sos);
    RUN_TEST(test_order_is_short_long_short);
    RUN_TEST(test_silent_after_the_pattern_and_repeats);
    RUN_TEST(test_millis_wrap_does_not_break_the_period);
    return UNITY_END();
}

#ifdef TREKLINK_HOST_TEST
int main()
{
    return runAll();
}
#else
#include <Arduino.h>
void setup()
{
    exit(runAll());
}
void loop() {}
#endif
