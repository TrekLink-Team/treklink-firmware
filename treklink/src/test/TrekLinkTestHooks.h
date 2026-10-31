/*
 * TrekLink test hooks: press a button from the bench computer (specs/architecture tasks 10).
 *
 * Diagnostic images only: compiled when the env adds -D TREKLINK_TEST_HOOKS=1, never in a release
 * image. A text message from the local client to this node itself, for example
 * "tk-test:press 4 3500", pulls that button's GPIO low for the given milliseconds, exactly what the
 * physical button does (every TrekLink button is active low with a pull-up). The pin is only ever
 * driven low through open drain, so a real press at the same time cannot short anything.
 *
 * A test run that uses this says so per behaviour (chapter 17 §3 rule 1).
 */
#pragma once

#include "Capabilities.h"

#if defined(TREKLINK_TEST_HOOKS) && TREKLINK_TEST_HOOKS

#include "Observer.h"
#include "SinglePortModule.h"
#include "concurrency/OSThread.h"
#include "input/InputBroker.h"

class TrekLinkTestHooks : public SinglePortModule, private concurrency::OSThread
{
  public:
    TrekLinkTestHooks();

  protected:
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    int32_t runOnce() override;

  private:
    // Logs every stock input event, so a test can see that a pressed button reached the InputBroker.
    int onInput(const InputEvent *event);
    CallbackObserver<TrekLinkTestHooks, const InputEvent *> inputObserver =
        CallbackObserver<TrekLinkTestHooks, const InputEvent *>(this, &TrekLinkTestHooks::onInput);
    int pressPin = -1;
    uint32_t releaseAt = 0;
    static bool pinAllowed(int pin);
    void logState();
};

#endif
