/*
 * TrekLink MQTT override (specs/architecture design §4.2 O-08 and O-09; specs/onboard-queue).
 *
 * Stock MQTT keeps at most 16 packets in RAM while the link is down and drops the oldest first, so an
 * SOS is the first thing lost (firmware ground truth §4.1). TrekLink replaces that policy without
 * editing MQTT.cpp: the linker routes every call of MQTT::onSend() from Router.cpp through
 * treklinkMqttOnSend() (-Wl,--wrap, set in the variant's platformio.ini). While the link is up and the
 * TrekLink queue is empty the stock function runs unchanged; otherwise the packet is encoded the way
 * stock encodes it and kept in the durable queue, which TrekLinkMqttDrain publishes through the public
 * MQTT::publish() once the link is back.
 */
#pragma once

#include "queue/TrekLinkEventQueue.h"

#if TREKLINK_OQ_ACTIVE

#include "concurrency/OSThread.h"

class TrekLinkMqttDrain : public concurrency::OSThread
{
  public:
    TrekLinkMqttDrain();

    TrekLinkEventQueue queue;

  protected:
    int32_t runOnce() override;

  private:
    bool linkWasUp = false;
    void publishHealth();
};

extern TrekLinkMqttDrain *trekLinkMqttDrain;

#endif // TREKLINK_OQ_ACTIVE
