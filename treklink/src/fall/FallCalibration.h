// Platform-free gyro calibration core (specs/fall-detection REQ-FALL-CAL-01, design §5; findings F6, F15).
// It collects samples in g and rad/s, accepts the run only if the unit lay still and level, and then
// yields the per-axis gyro bias and the gyro stillness threshold. Free-fall and impact thresholds are
// fixed and not calibrated.

#pragma once

#include <cmath>
#include <cstdint>

namespace treklink
{
namespace fall
{

struct CalibrationLimits {
    uint16_t samples = 250;          // 5 s at the 20 ms period
    float meanToleranceG = 0.1f;     // |mean |a| - 1 g| must stay below this
    float maxAccelStdG = 0.05f;      // every accelerometer axis
    float maxGyroStdRads = 0.1f;     // every gyroscope axis
    float stillFactor = 3.0f;        // threshold = factor x mean per-axis gyro std
    float stillMinRads = 0.05f;
    float stillMaxRads = 0.3f;
    float defaultStillRads = 1.0f;   // used with no accepted calibration
};

enum class CalVerdict : uint8_t { COLLECTING, ACCEPTED, REJECTED_MOVING, REJECTED_MEAN_NOT_1G, REJECTED_GYRO_NOISE };

struct CalibrationResult {
    float bias[3] = {0, 0, 0}; // rad/s
    float stillRads = 1.0f;
    float accelStdG = 0, gyroStdRads = 0, meanG = 0; // for the log
};

class FallCalibration
{
  public:
    explicit FallCalibration(const CalibrationLimits &limits = CalibrationLimits()) : lim(limits) { restart(); }

    void restart()
    {
        n = 0;
        for (int i = 0; i < 3; i++)
            sa[i] = saa[i] = sg[i] = sgg[i] = 0;
        smag = 0;
    }

    // accelG: acceleration per axis in g; gyroRads: angular rate per axis in rad/s.
    CalVerdict add(const float accelG[3], const float gyroRads[3])
    {
        for (int i = 0; i < 3; i++) {
            sa[i] += accelG[i];
            saa[i] += (double)accelG[i] * accelG[i];
            sg[i] += gyroRads[i];
            sgg[i] += (double)gyroRads[i] * gyroRads[i];
        }
        smag += std::sqrt((double)accelG[0] * accelG[0] + (double)accelG[1] * accelG[1] + (double)accelG[2] * accelG[2]);
        if (++n < lim.samples)
            return CalVerdict::COLLECTING;
        return finish();
    }

    const CalibrationResult &result() const { return res; }
    const CalibrationLimits &limits() const { return lim; }

  private:
    static double stddev(double s, double ss, double count)
    {
        double m = s / count;
        double v = ss / count - m * m;
        return v > 0 ? std::sqrt(v) : 0;
    }

    CalVerdict finish()
    {
        double c = n;
        double maxA = 0, maxG = 0, sumG = 0;
        for (int i = 0; i < 3; i++) {
            double a = stddev(sa[i], saa[i], c), g = stddev(sg[i], sgg[i], c);
            maxA = a > maxA ? a : maxA;
            maxG = g > maxG ? g : maxG;
            sumG += g;
        }
        res.meanG = (float)(smag / c);
        res.accelStdG = (float)maxA;
        res.gyroStdRads = (float)maxG;
        if (maxA >= lim.maxAccelStdG)
            return CalVerdict::REJECTED_MOVING;
        if (std::fabs(res.meanG - 1.0f) >= lim.meanToleranceG)
            return CalVerdict::REJECTED_MEAN_NOT_1G;
        if (maxG >= lim.maxGyroStdRads)
            return CalVerdict::REJECTED_GYRO_NOISE;
        for (int i = 0; i < 3; i++)
            res.bias[i] = (float)(sg[i] / c);
        float t = (float)(lim.stillFactor * sumG / 3.0);
        res.stillRads = t < lim.stillMinRads ? lim.stillMinRads : t > lim.stillMaxRads ? lim.stillMaxRads : t;
        return CalVerdict::ACCEPTED;
    }

    CalibrationLimits lim;
    CalibrationResult res;
    uint16_t n = 0;
    double sa[3], saa[3], sg[3], sgg[3], smag;
};

} // namespace fall
} // namespace treklink
