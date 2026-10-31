// Platform-free fall detector core (specs/fall-detection design §2, REQ-FALL-SIG-01, REQ-FALL-STL-01).
// It takes one sample at a time with its own timestamp and holds no Arduino state, so the native test
// (test/treklink_fall) and the replay tool run the same code as the firmware.

#pragma once

#include <cstdint>

namespace treklink
{
namespace fall
{

struct DetectorConfig {
    float freeFallG = 0.5f;          // a sample below this is part of a free fall
    uint32_t freeFallMinMs = 300;    // free fall that long starts the check when it ends
    float impactG = 3.0f;            // two consecutive samples above this start the check
    uint32_t settleMs = 2000;        // ignored after the check starts (impact, bounce)
    uint32_t stillMs = 10000;        // continuous stillness that raises the pre-alarm
    uint32_t watchMs = 20000;        // window after the settle in which a still run must start
    float stillAccelBandG = 0.2f;    // still: |a - 1 g| below this
    float stillGyroRads = 1.0f;      // still: |w - bias| below this (set from calibration)
};

enum class Event : uint8_t { NONE, CHECK_STARTED, USER_OK, RAISE_PREALARM };
enum class Cause : uint8_t { NONE, FREE_FALL, IMPACT };
enum class Phase : uint8_t { IDLE, FREE_FALL, SETTLE, WATCH };

class FallDetector
{
  public:
    explicit FallDetector(const DetectorConfig &config = DetectorConfig()) : cfg(config) {}

    // a: acceleration magnitude in g; w: bias-corrected angular rate magnitude in rad/s.
    Event step(uint32_t tMs, float a, float w)
    {
        bool impactNow = a > cfg.impactG;
        bool impactPair = impactNow && prevImpact && havePrev;
        prevImpact = impactNow;
        havePrev = true;

        switch (phase_) {
        case Phase::IDLE:
            if (impactPair)
                return start(tMs, Cause::IMPACT);
            if (a < cfg.freeFallG) {
                freeFallStart = tMs;
                phase_ = Phase::FREE_FALL;
            }
            return Event::NONE;

        case Phase::FREE_FALL:
            if (impactPair)
                return start(tMs, Cause::IMPACT);
            if (a >= cfg.freeFallG) {
                lastFreeFallMs_ = tMs - freeFallStart;
                freeFallEnd = tMs;
                if (lastFreeFallMs_ >= cfg.freeFallMinMs)
                    return start(tMs, Cause::FREE_FALL);
                phase_ = Phase::IDLE;
            }
            return Event::NONE;

        case Phase::SETTLE:
            peak(a);
            if (tMs - checkStart >= cfg.settleMs) {
                phase_ = Phase::WATCH;
                watchStart = tMs;
                stillOpen = false;
                stepWatch(tMs, a, w);
            }
            return Event::NONE;

        case Phase::WATCH:
            peak(a);
            return stepWatch(tMs, a, w);
        }
        return Event::NONE;
    }

    bool isStill(float a, float w) const
    {
        float dev = a - 1.0f;
        if (dev < 0)
            dev = -dev;
        return dev < cfg.stillAccelBandG && w < cfg.stillGyroRads;
    }

    void reset()
    {
        phase_ = Phase::IDLE;
        stillOpen = false;
        havePrev = false;
        prevImpact = false;
    }

    void setStillGyro(float rads) { cfg.stillGyroRads = rads; }
    const DetectorConfig &config() const { return cfg; }
    Phase phase() const { return phase_; }
    Cause cause() const { return cause_; }
    uint32_t lastFreeFallMs() const { return lastFreeFallMs_; }
    float peakG() const { return peakG_; }

  private:
    Event start(uint32_t tMs, Cause c)
    {
        cause_ = c;
        // For the log: the free fall that led into an impact, if one ended in the last second.
        if (c == Cause::IMPACT)
            lastFreeFallMs_ = phase_ == Phase::FREE_FALL ? tMs - freeFallStart : (tMs - freeFallEnd < 1000 ? lastFreeFallMs_ : 0);
        checkStart = tMs;
        peakG_ = 0;
        phase_ = Phase::SETTLE;
        return Event::CHECK_STARTED;
    }

    void peak(float a)
    {
        if (a > peakG_)
            peakG_ = a;
    }

    Event stepWatch(uint32_t tMs, float a, float w)
    {
        bool windowOver = tMs - watchStart >= cfg.watchMs;
        if (isStill(a, w)) {
            if (!stillOpen) {
                if (windowOver) {
                    reset();
                    return Event::USER_OK;
                }
                stillOpen = true;
                stillStart = tMs;
            }
            if (tMs - stillStart >= cfg.stillMs) {
                reset();
                return Event::RAISE_PREALARM;
            }
            return Event::NONE;
        }
        // A move restarts the run (REQ-FALL-STL-01.3). An open run past the window may still finish,
        // so only a moment with no open run ends the check (design §2).
        stillOpen = false;
        if (windowOver) {
            reset();
            return Event::USER_OK;
        }
        return Event::NONE;
    }

    DetectorConfig cfg;
    Phase phase_ = Phase::IDLE;
    Cause cause_ = Cause::NONE;
    uint32_t freeFallStart = 0;
    uint32_t freeFallEnd = 0;
    uint32_t checkStart = 0;
    uint32_t watchStart = 0;
    uint32_t stillStart = 0;
    uint32_t lastFreeFallMs_ = 0;
    float peakG_ = 0;
    bool stillOpen = false;
    bool prevImpact = false;
    bool havePrev = false;
};

} // namespace fall
} // namespace treklink
