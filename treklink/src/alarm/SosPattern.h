/*
 * The common SOS alarm pattern (specs/sos/requirements.md REQ-SOS-ALM-01). Platform-free, so the
 * native test in test/treklink_alarm checks the timing on the host.
 *
 * Morse "SOS" in 120 ms units: short mark 1 unit, long mark 3, gap inside a letter 1, gap between
 * letters 3; 27 units (3.24 s), then off until the 5 s period ends.
 */
#pragma once

#include <stdint.h>

namespace treklink
{
namespace alarm
{

static const uint32_t SOS_UNIT_MS = 120;
static const uint32_t SOS_PERIOD_MS = 5000;
static const uint8_t SOS_UNITS = 27;

/// True while the SOS pattern has a mark (output on) at elapsedMs after the alarm started.
inline bool sosMarkAt(uint32_t elapsedMs)
{
    static const uint8_t MARKS[SOS_UNITS] = {1, 0, 1, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 1};
    const uint32_t unit = (elapsedMs % SOS_PERIOD_MS) / SOS_UNIT_MS;
    return unit < SOS_UNITS && MARKS[unit] != 0;
}

} // namespace alarm
} // namespace treklink
