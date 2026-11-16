#pragma once

#if defined(ARCH_PORTDUINO) && defined(TREKLINK_SCREENCAP)

#include <OLEDDisplay.h>
#include <mutex>
#include <stdint.h>
#include <vector>

/**
 * TrekLink screen capture (tools/screencap). An OLEDDisplay with no panel
 * behind it: every frame the UI pushes is kept in memory, so the native build
 * can save the real device screens as images without hardware.
 */
class CaptureDisplay : public OLEDDisplay
{
  public:
    explicit CaptureDisplay(OLEDDISPLAY_GEOMETRY geometry);

    void display() override;

    /** Writes the last pushed frame as a binary PBM (P4) where a set bit is a lit
     * pixel. False before the first frame. */
    bool savePbm(const char *path);

    /** The display the capture build created, for the command thread. */
    static CaptureDisplay *instance;

  protected:
    int getBufferOffset() override { return 0; }
    bool connect() override { return true; }

  private:
    std::mutex frameLock;
    std::vector<uint8_t> lastFrame;
};

#endif
