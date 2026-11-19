/*
 * FallSensorUnits: platform-free unit conversions for the fall sensor adapters.
 *
 * FallSensorInterface returns acceleration in m/s^2 and rotation in rad/s
 * (src/modules/FallSensorInterface.h:11-13). Each adapter converts its library's
 * native unit once, with these functions, so the conversion is covered by
 * `pio test -e native -f test_fall_sensor_units` without any IMU library.
 *
 * Standard headers only (docs/coding-conventions.md section 9).
 */

#pragma once

namespace treklink
{
namespace fall
{

/// Standard gravity, m/s^2 per g (ISO 80000-3).
constexpr float kStandardGravityMps2 = 9.80665f;

/// Radians per degree.
constexpr float kRadPerDeg = 3.14159265358979323846f / 180.0f;

/// Acceleration in g to m/s^2.
constexpr float gToMps2(float accelG)
{
    return accelG * kStandardGravityMps2;
}

/// Rotation rate in degrees per second to rad/s.
constexpr float dpsToRadS(float rateDps)
{
    return rateDps * kRadPerDeg;
}

} // namespace fall
} // namespace treklink
