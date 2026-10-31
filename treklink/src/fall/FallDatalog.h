// IMU datalog tap for fall-detection tuning (specs/fall-detection REQ-FALL-LOG-01).
//
// A datalog image (tk.py build --datalog, which adds -DTREKLINK_IMU_STREAM=1 to a diagnostic image)
// prints every sample the fall detector acts on, so `tk.py imu` can record it on a laptop:
//   #IMUHDR,seq,t_ms,ax,ay,az,gx,gy,gz,accel_g,gyro_rads,state   once, when the sensor comes up
//   #IMU,<seq>,<t_ms>,<ax>,<ay>,<az>,<gx>,<gy>,<gz>,<|a| g>,<|w| rad/s>,<state>
//   #EVT,<seq>,<t_ms>,<name>,<detail>                           at every state change or event
// Acceleration is in m/s2 and angular rate in rad/s, the FallSensorInterface units. Without the flag
// every macro compiles to nothing.

#pragma once

#ifndef TREKLINK_IMU_STREAM
#define TREKLINK_IMU_STREAM 0
#endif

#if TREKLINK_IMU_STREAM

#include "fall/FallSensorInterface.h"
#include <cstdint>

namespace treklink
{
namespace fall
{
void datalogBegin(const char *sensorName);
void datalogSample(const SensorVec3 &accel, const SensorVec3 &gyro, float accelG, float gyroRads, uint8_t state);
void datalogEvent(const char *name, const char *detail);
} // namespace fall
} // namespace treklink

#define TREKLINK_IMU_BEGIN(name) treklink::fall::datalogBegin(name)
#define TREKLINK_IMU_SAMPLE(a, g, ag, gr, st) treklink::fall::datalogSample(a, g, ag, gr, st)
#define TREKLINK_IMU_EVENT(name, detail) treklink::fall::datalogEvent(name, detail)

#else

#define TREKLINK_IMU_BEGIN(name)                                                                                                 \
    do {                                                                                                                         \
    } while (0)
#define TREKLINK_IMU_SAMPLE(a, g, ag, gr, st)                                                                                    \
    do {                                                                                                                         \
    } while (0)
#define TREKLINK_IMU_EVENT(name, detail)                                                                                         \
    do {                                                                                                                         \
    } while (0)

#endif
