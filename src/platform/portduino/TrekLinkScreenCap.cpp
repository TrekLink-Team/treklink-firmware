#include "configuration.h"

#if defined(ARCH_PORTDUINO) && defined(TREKLINK_SCREENCAP)

#include "TrekLinkScreenCap.h"
#include "concurrency/OSThread.h"
#include "graphics/CaptureDisplay.h"
#include "motion/ScreenCapMotionSensor.h"
#include <Arduino.h>
#include <PortduinoGPIO.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <unistd.h>

namespace
{

/** A button pin the command thread drives. TrekLink buttons are active low with
 * a pull-up: released reads HIGH. */
class ScreenCapButtonPin : public GPIOPin
{
  public:
    ScreenCapButtonPin(pin_size_t n, const char *name) : GPIOPin(n, name)
    {
        setSilent();
        writePin(HIGH);
    }

    void setPressed(bool pressed) { level = pressed ? LOW : HIGH; }

  protected:
    PinStatus readPinHardware() override { return level; }

  private:
    volatile PinStatus level = HIGH;
};

struct NamedButton {
    const char *name;
    int pin;
    ScreenCapButtonPin *gpio;
};

NamedButton buttons[] = {
#if defined(BUTTON_PIN_SOS)
    {"menu", BUTTON_PIN, nullptr},
    {"up", BUTTON_PIN_UP, nullptr},
    {"down", BUTTON_PIN_DOWN, nullptr},
    {"sos", BUTTON_PIN_SOS, nullptr},
#else
    {"user", BUTTON_PIN, nullptr},
#endif
};

ScreenCapButtonPin *findButton(const char *name)
{
    for (auto &b : buttons)
        if (strcmp(b.name, name) == 0)
            return b.gpio;
    return nullptr;
}

class ScreenCapThread : public concurrency::OSThread
{
  public:
    ScreenCapThread() : OSThread("ScreenCap") {}

  protected:
    int32_t runOnce() override
    {
        if (held && (int32_t)(millis() - releaseAt) >= 0) {
            held->setPressed(false);
            held = nullptr;
            puts("SCREENCAP released");
            fflush(stdout);
        }

        struct pollfd in = {STDIN_FILENO, POLLIN, 0};
        while (poll(&in, 1, 0) > 0) {
            char c;
            // The script that started us is gone (stdin closed): exit instead of running on as an orphan.
            if (!(in.revents & POLLIN) || read(STDIN_FILENO, &c, 1) != 1)
                _exit(0);
            if (c == '\n') {
                handle(line);
                line.clear();
            } else if (line.size() < 512) {
                line += c;
            }
        }
        return 10;
    }

  private:
    std::string line;
    ScreenCapButtonPin *held = nullptr;
    uint32_t releaseAt = 0;

    void handle(const std::string &cmd)
    {
        char arg[256] = {0};
        unsigned ms = 0;
        if (sscanf(cmd.c_str(), "press %63s %u", arg, &ms) == 2) {
            ScreenCapButtonPin *pin = findButton(arg);
            if (!pin || held) {
                printf("SCREENCAP error %s\n", pin ? "a button is already held" : "unknown button");
            } else {
                pin->setPressed(true);
                held = pin;
                releaseAt = millis() + ms;
                printf("SCREENCAP pressed %s\n", arg);
            }
        } else if (sscanf(cmd.c_str(), "shot %255s", arg) == 1) {
            CaptureDisplay *display = CaptureDisplay::instance;
            bool ok = display && display->savePbm(arg);
            printf("SCREENCAP %s %s\n", ok ? "shot" : "error shot", arg);
        } else if (sscanf(cmd.c_str(), "heading %u", &ms) == 1) {
#if !MESHTASTIC_EXCLUDE_I2C
            if (ScreenCapMotionSensor::instance) {
                ScreenCapMotionSensor::instance->setHeading((float)(ms % 360));
                printf("SCREENCAP heading %u\n", ms % 360);
            } else
#endif
                printf("SCREENCAP error no motion sensor in this build\n");
        } else if (cmd == "quit") {
            puts("SCREENCAP bye");
            fflush(stdout);
            _exit(0);
        } else if (!cmd.empty()) {
            printf("SCREENCAP error unknown command\n");
        }
        fflush(stdout);
    }
};

} // namespace

void screenCapBindButtons()
{
    for (auto &b : buttons) {
        b.gpio = new ScreenCapButtonPin(b.pin, b.name);
        gpioBind(b.gpio);
    }
}

void screenCapStart()
{
    static ScreenCapThread *thread = nullptr;
    if (!thread)
        thread = new ScreenCapThread();
}

#endif
