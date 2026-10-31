#include "fall/FallDatalog.h"

#if TREKLINK_IMU_STREAM

#include "SerialConsole.h"
#include "configuration.h"

namespace treklink
{
namespace fall
{
static uint32_t seq = 0;

void datalogBegin(const char *sensorName)
{
    seq = 0;
    consolePrintf("#IMUHDR,seq,t_ms,ax,ay,az,gx,gy,gz,accel_g,gyro_rads,state\n");
    datalogEvent("STREAM_START", sensorName);
}

void datalogSample(const SensorVec3 &accel, const SensorVec3 &gyro, float accelG, float gyroRads, uint8_t state)
{
    consolePrintf("#IMU,%lu,%lu,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%u\n", (unsigned long)seq++, (unsigned long)millis(),
                  accel.x, accel.y, accel.z, gyro.x, gyro.y, gyro.z, accelG, gyroRads, (unsigned)state);
}

void datalogEvent(const char *name, const char *detail)
{
    consolePrintf("#EVT,%lu,%lu,%s,%s\n", (unsigned long)seq, (unsigned long)millis(), name, detail ? detail : "");
}
} // namespace fall
} // namespace treklink

#endif
