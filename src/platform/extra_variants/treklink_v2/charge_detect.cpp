#include "configuration.h"

#if defined(TREKLINK_V2) && defined(TREKLINK_CHARGE_FROM_VOLTAGE)
#include "charge_detect.h"
#include "concurrency/OSThread.h"
#include "power.h"
#include <algorithm>
#include <soc/usb_serial_jtag_struct.h>

namespace
{

// Bench values, a268/a278/a294, 2026-11-02: steps of 20 to 36 mV, noise about 2 mV between 2 s samples.
constexpr int32_t kPeriodMs = 2000;
constexpr int kStepMv = 15;         // smallest step treated as a charger connected or removed
constexpr int kHoldSamples = 3;     // a step must hold this many samples (6 s); filters TX dips and spikes
constexpr int kChargedMv = 4150;    // at or above this with external power: charged, not charging
constexpr int kReadsPerSample = 16; // ADC reads averaged per sample

class ChargeDetect : public concurrency::OSThread
{
  public:
    ChargeDetect() : OSThread("TrekLinkCharge") {}

    bool externalPower = false;
    int levelMv = 0;

  protected:
    int32_t runOnce() override
    {
        bool changed = false;

        // A USB host sends a start-of-frame every millisecond: a moving frame index means a PC is attached.
        uint32_t frame = USB_SERIAL_JTAG.fram_num.sof_frame_index;
        bool host = haveFrame && frame != lastFrame;
        lastFrame = frame;
        haveFrame = true;

        uint32_t sum = 0;
        for (int i = 0; i < kReadsPerSample; i++)
            sum += analogReadMilliVolts(BATTERY_PIN);
        recent[recentCount++ % 3] = (int)(sum / kReadsPerSample * ADC_MULTIPLIER);
        if (recentCount < 3)
            return kPeriodMs;
        int mv = median3(recent[0], recent[1], recent[2]); // drops single-sample spikes
        levelMv = mv;

        if (referenceMv == 0)
            referenceMv = mv;
        int delta = mv - referenceMv;
        if (delta >= kStepMv) {
            up++;
            down = 0;
        } else if (delta <= -kStepMv) {
            down++;
            up = 0;
        } else {
            up = down = 0;
            referenceMv += (mv - referenceMv) / 4; // follow the slow charge or discharge drift
        }

        if (host && !externalPower) {
            externalPower = true;
            changed = true;
        } else if (up >= kHoldSamples && !externalPower) {
            externalPower = true;
            changed = true;
        } else if (down >= kHoldSamples && externalPower && !host) {
            externalPower = false;
            changed = true;
        }
        if (up >= kHoldSamples || down >= kHoldSamples) {
            referenceMv = mv;
            up = down = 0;
        }

        if (changed) {
            LOG_INFO("TrekLink charge: external power %s (%d mV%s)", externalPower ? "connected" : "removed", mv,
                     host ? ", USB host" : "");
            if (power)
                power->setIntervalFromNow(0); // publish the new power status now instead of in up to 20 s
        }
        return kPeriodMs;
    }

  private:
    int recent[3] = {0, 0, 0};
    uint32_t recentCount = 0;
    int referenceMv = 0;
    int up = 0;
    int down = 0;
    uint32_t lastFrame = 0;
    bool haveFrame = false;

    static int median3(int a, int b, int c) { return std::max(std::min(a, b), std::min(std::max(a, b), c)); }
};

ChargeDetect *detector = nullptr;

} // namespace

void treklinkChargeDetectBegin()
{
    if (!detector)
        detector = new ChargeDetect();
}

bool treklinkExternalPower()
{
    return detector && detector->externalPower;
}

bool treklinkCharging()
{
    return detector && detector->externalPower && detector->levelMv < kChargedMv;
}
#endif
