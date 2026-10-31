/*
 * Plays the common SOS pattern on every local alarm output the variant has: buzzer, vibration motor
 * and SOS LED (specs/sos/requirements.md REQ-SOS-ALM-01 and -02).
 */
#pragma once

#include "Capabilities.h"
#include "alarm/BuzzerManager.h"
#include "concurrency/OSThread.h"

class AlarmPlayer : public concurrency::OSThread
{
  public:
    AlarmPlayer();

    /// Start the SOS pattern for owner (OWNER_SOS or OWNER_FALL); a second start keeps the running one.
    void startSOS(uint8_t owner);
    /// Stop the pattern, turn every output off and release the buzzer.
    void stop(uint8_t owner);
    bool isPlaying() const { return playing; }
    /// While true the pattern leaves the vibration motor to a confirmation sequence (REQ-SOS-ALM-01.5).
    void holdVibrator(bool hold) { vibratorHeld = hold; }

  protected:
    int32_t runOnce() override;

  private:
    bool playing = false;
    bool vibratorHeld = false;
    bool markOn = false;
    uint8_t owner = 0;
    uint32_t startMs = 0;
    void setOutputs(bool on);
};

extern AlarmPlayer *alarmPlayer;
