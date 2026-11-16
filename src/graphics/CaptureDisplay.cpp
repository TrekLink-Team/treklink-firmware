#include "configuration.h"

#if defined(ARCH_PORTDUINO) && defined(TREKLINK_SCREENCAP)

#include "CaptureDisplay.h"
#include <algorithm>
#include <stdio.h>

CaptureDisplay *CaptureDisplay::instance = nullptr;

CaptureDisplay::CaptureDisplay(OLEDDISPLAY_GEOMETRY geometry)
{
    setGeometry(geometry);
    instance = this;
}

void CaptureDisplay::display()
{
    std::lock_guard<std::mutex> guard(frameLock);
    lastFrame.assign(buffer, buffer + displayBufferSize);
}

bool CaptureDisplay::savePbm(const char *path)
{
    std::vector<uint8_t> frame;
    {
        std::lock_guard<std::mutex> guard(frameLock);
        frame = lastFrame;
    }
    const int w = getWidth();
    const int h = getHeight();
    if (frame.size() < (size_t)(w * ((h + 7) / 8)))
        return false;

    FILE *out = fopen(path, "wb");
    if (!out)
        return false;
    fprintf(out, "P4\n%d %d\n", w, h);
    // The OLED buffer is organised in pages: byte (x + (y / 8) * w), bit (y % 8).
    // PBM wants rows, MSB first.
    const int rowBytes = (w + 7) / 8;
    std::vector<uint8_t> row(rowBytes);
    for (int y = 0; y < h; y++) {
        std::fill(row.begin(), row.end(), 0);
        for (int x = 0; x < w; x++) {
            if (frame[x + (y / 8) * w] & (1 << (y % 8)))
                row[x / 8] |= 0x80 >> (x % 8);
        }
        fwrite(row.data(), 1, rowBytes, out);
    }
    fclose(out);
    return true;
}

#endif
