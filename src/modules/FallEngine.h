#pragma once

// Rule-based fall engine (v2 first). Pure logic with no Arduino dependency, so the host replay test can run it on recorded
// IMU captures. It replaces the single "10 s of mechanical stillness" test, which never passed for a living person: breathing,
// moving in pain or convulsing broke it, while a unit set down on a table passed it.
//
// Per sample: |a| for free fall and impact triggers. Per 1 s block: the spread of |a|, its zero-crossing rate, the gyro
// magnitude (bias learnt while the unit is mechanically still) and the gravity direction. Each block is QUIET (breathing
// allowed), GAIT (walking or jogging rhythm), SHAKE (fast rhythmic shaking, as in a convulsion) or ACTIVE. After a trigger
// and a 2 s settle, the engine watches until 60 s pass with no new trigger:
//   - 6 GAIT blocks in the last 8: the person is up and walking, no alarm;
//   - 6 SHAKE blocks in the last 10: pre-alarm (convulsion);
//   - 10 QUIET blocks in the last 15, when the unit tilted 35 degrees or more from its pre-trigger direction, or the trigger
//     was a free fall of 300 ms or an impact of 8 g: pre-alarm (lying still, breathing);
//   - tilted on 8 of the last 10 blocks 45 s after the trigger, with no walking: pre-alarm (down and moving in pain, or
//     reached for the unit).
// Thresholds come from the labelled v2 bench captures of 2026-11-09 (about 34 Hz there; the module samples at 50 Hz).

#include <cmath>
#include <cstdint>

namespace treklink
{

class FallEngine
{
  public:
    enum Event : uint8_t { NONE = 0, TRIGGER, RECOVERED, PREALARM };
    enum Reason : uint8_t { R_NONE = 0, R_FREEFALL, R_IMPACT, R_GAIT, R_WINDOW_END, R_SHAKING, R_STILL, R_DOWN };

    struct Params {
        float ffG = 0.5f;
        uint32_t ffMs = 300;
        float impact2G = 3.0f; // two consecutive samples at or above
        float impact1G = 4.0f; // one sample at or above
        float hardG = 8.0f;
        uint32_t settleMs = 2000;
        uint32_t watchMs = 60000;
        uint32_t timeoutMs = 45000;
        float quietStd = 0.03f;
        float quietGyro = 0.35f;
        float shakeZc = 8.0f;
        float shakeStd = 0.25f;
        float gaitZcMin = 1.5f;
        float gaitZcMax = 7.5f;
        float gaitStd = 0.08f;
        float biasStd = 0.01f;
        float tiltDeg = 35.0f;
    };

    Reason reason = R_NONE; // reason of the last non-NONE event
    bool watching() const { return watch_; }

    // Forget any free fall or watch in progress; call when the module resumes after a pre-alarm or SOS.
    void reset()
    {
        inFf_ = prevHi_ = false;
        watch_ = false;
        n_ = 0;
        haveBlk_ = false;
        sx_ = sy_ = sz_ = 0;
    }

    // t in ms, accel in m/s2, gyro in rad/s. Returns the most significant event of this sample.
    Event update(uint32_t t, float ax, float ay, float az, float gx, float gy, float gz)
    {
        Event out = NONE;
        const float a = std::sqrt(ax * ax + ay * ay + az * az) / 9.80665f;

        // Triggers
        bool trig = false, isImpact = false, ffEnded = false;
        if (a < p_.ffG) {
            if (!inFf_) {
                inFf_ = true;
                ffStart_ = t;
            }
        } else {
            if (inFf_ && t - ffStart_ >= p_.ffMs) {
                trig = true;
                ffEnded = true;
            }
            inFf_ = false;
        }
        const bool hi2 = a >= p_.impact2G;
        if ((hi2 && prevHi_) || a >= p_.impact1G) {
            trig = true;
            isImpact = true;
        }
        prevHi_ = hi2;
        if (trig) {
            if (!watch_)
                startWatch(t);
            lastTrigT_ = t; // a later trigger keeps the watch open for another full window
            if (isImpact && a > peak_)
                peak_ = a;
            if (ffEnded)
                freefall_ = true;
            reason = isImpact ? R_IMPACT : R_FREEFALL;
            out = TRIGGER;
        }

        // 1 s blocks
        if (!haveBlk_) {
            haveBlk_ = true;
            blkT0_ = t;
        }
        if (n_ < MAX_BLK) {
            mag_[n_] = a;
            sx_ += ax;
            sy_ += ay;
            sz_ += az;
            gxr_[n_] = gx;
            gyr_[n_] = gy;
            gzr_[n_] = gz;
            tFirst_ = n_ == 0 ? t : tFirst_;
            tLast_ = t;
            n_++;
        }
        if (t - blkT0_ >= 1000) {
            Event e = closeBlock(t);
            if (e != NONE)
                out = e;
        }
        return out;
    }

  private:
    enum Cls : uint8_t { C_QUIET, C_GAIT, C_SHAKE, C_ACTIVE };
    static constexpr int MAX_BLK = 64; // about 51 samples a block at 20 ms
    static constexpr int HIST = 16;    // block classes kept during a watch
    static constexpr int GRAV = 8;     // gravity directions kept for the pre-trigger baseline

    Params p_;
    // trigger state
    bool inFf_ = false, prevHi_ = false;
    uint32_t ffStart_ = 0;
    // block accumulators
    bool haveBlk_ = false;
    uint32_t blkT0_ = 0, tFirst_ = 0, tLast_ = 0;
    int n_ = 0;
    float mag_[MAX_BLK];
    float gxr_[MAX_BLK], gyr_[MAX_BLK], gzr_[MAX_BLK];
    float sx_ = 0, sy_ = 0, sz_ = 0;
    float bias_[3] = {0, 0, 0};
    float grav_[GRAV][3];
    int gravN_ = 0;
    // watch state
    bool watch_ = false, freefall_ = false, haveBase_ = false;
    uint32_t trigT_ = 0, lastTrigT_ = 0;
    float peak_ = 0;
    float base_[3] = {0, 0, 1};
    uint8_t cls_[HIST];
    uint8_t tiltOk_[HIST];
    int clsN_ = 0;

    void startWatch(uint32_t t)
    {
        watch_ = true;
        freefall_ = false;
        peak_ = 0;
        clsN_ = 0;
        trigT_ = t;
        lastTrigT_ = t;
        // baseline: mean gravity direction of the kept blocks except the newest
        int m = gravN_ > 1 ? gravN_ - 1 : gravN_;
        haveBase_ = m > 0;
        if (haveBase_) {
            float v[3] = {0, 0, 0};
            for (int i = 0; i < m; i++)
                for (int k = 0; k < 3; k++)
                    v[k] += grav_[i][k];
            float nv = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
            if (nv <= 0)
                nv = 1;
            for (int k = 0; k < 3; k++)
                base_[k] = v[k] / nv;
        }
    }

    int countLast(int win, uint8_t c) const
    {
        int k = 0;
        for (int i = clsN_ - win < 0 ? 0 : clsN_ - win; i < clsN_; i++)
            k += cls_[i] == c;
        return k;
    }

    int tiltLast(int win) const
    {
        int k = 0;
        for (int i = clsN_ - win < 0 ? 0 : clsN_ - win; i < clsN_; i++)
            k += tiltOk_[i];
        return k;
    }

    Event closeBlock(uint32_t t)
    {
        const int n = n_;
        Event out = NONE;
        if (n >= 5) {
            float m = 0;
            for (int i = 0; i < n; i++)
                m += mag_[i];
            m /= n;
            float var = 0;
            for (int i = 0; i < n; i++)
                var += (mag_[i] - m) * (mag_[i] - m);
            const float sd = std::sqrt(var / n);
            float dur = (tLast_ - tFirst_) / 1000.0f;
            if (dur < 0.2f)
                dur = 0.2f;
            int zc = 0;
            for (int i = 1; i < n; i++)
                zc += (mag_[i - 1] - m) * (mag_[i] - m) < 0;
            const float zcr = zc / dur;
            if (sd < p_.biasStd) {
                float b[3] = {0, 0, 0};
                for (int i = 0; i < n; i++) {
                    b[0] += gxr_[i];
                    b[1] += gyr_[i];
                    b[2] += gzr_[i];
                }
                for (int k = 0; k < 3; k++)
                    bias_[k] = 0.9f * bias_[k] + 0.1f * (b[k] / n);
            }
            float w = 0;
            for (int i = 0; i < n; i++) {
                const float x = gxr_[i] - bias_[0], y = gyr_[i] - bias_[1], z = gzr_[i] - bias_[2];
                w += std::sqrt(x * x + y * y + z * z);
            }
            w /= n;
            uint8_t c;
            if (sd < p_.quietStd && w < p_.quietGyro)
                c = C_QUIET;
            else if (zcr >= p_.shakeZc && sd >= p_.shakeStd)
                c = C_SHAKE;
            else if (zcr >= p_.gaitZcMin && zcr <= p_.gaitZcMax && sd >= p_.gaitStd)
                c = C_GAIT;
            else
                c = C_ACTIVE;
            // gravity direction of this block
            float gn = std::sqrt(sx_ * sx_ + sy_ * sy_ + sz_ * sz_);
            if (gn <= 0)
                gn = 1;
            const float g[3] = {sx_ / gn, sy_ / gn, sz_ / gn};
            if (gravN_ < GRAV) {
                gravN_++;
            } else {
                for (int i = 1; i < GRAV; i++)
                    for (int k = 0; k < 3; k++)
                        grav_[i - 1][k] = grav_[i][k];
            }
            for (int k = 0; k < 3; k++)
                grav_[gravN_ - 1][k] = g[k];

            if (watch_ && t - trigT_ >= p_.settleMs)
                out = judge(t, c, g);
        }
        n_ = 0;
        sx_ = sy_ = sz_ = 0;
        blkT0_ = t;
        return out;
    }

    Event judge(uint32_t t, uint8_t c, const float g[3])
    {
        float tilt = 0;
        if (haveBase_) {
            float d = g[0] * base_[0] + g[1] * base_[1] + g[2] * base_[2];
            d = d > 1 ? 1 : (d < -1 ? -1 : d);
            tilt = std::acos(d) * 57.29578f;
        }
        if (clsN_ == HIST) {
            for (int i = 1; i < HIST; i++) {
                cls_[i - 1] = cls_[i];
                tiltOk_[i - 1] = tiltOk_[i];
            }
            clsN_--;
        }
        cls_[clsN_] = c;
        tiltOk_[clsN_] = tilt >= p_.tiltDeg ? 1 : 0;
        clsN_++;
        const bool tilted = tilt >= p_.tiltDeg;

        if (countLast(8, C_GAIT) >= 6) {
            watch_ = false;
            reason = R_GAIT;
            return RECOVERED;
        }
        if (countLast(10, C_SHAKE) >= 6) {
            watch_ = false;
            reason = R_SHAKING;
            return PREALARM;
        }
        if ((tilted || freefall_ || peak_ >= p_.hardG) && countLast(15, C_QUIET) >= 10) {
            watch_ = false;
            reason = R_STILL;
            return PREALARM;
        }
        if (tilted && t - trigT_ >= p_.timeoutMs && tiltLast(10) >= 8) {
            watch_ = false;
            reason = R_DOWN;
            return PREALARM;
        }
        if (t - lastTrigT_ >= p_.watchMs) {
            watch_ = false;
            reason = R_WINDOW_END;
            return RECOVERED;
        }
        return NONE;
    }
};

} // namespace treklink
