/*
 * TrekLink onboard-queue firmware adapter (specs/onboard-queue/design.md sections 0.1 and 2.5).
 *
 * Wires the platform-free QueueCore to LittleFS, to the SOS modules, and to the reboot and deep-sleep
 * observers. MQTT owns one instance and calls it from the three seams in MQTT.cpp.
 *
 * Enabled with -D TREKLINK_ONBOARD_QUEUE=1 in a variant's platformio.ini. With the flag off, or with MQTT
 * excluded, nothing here is compiled and MQTT keeps its stock queue (D-019, REQ-UBI-02, REQ-ERR-05).
 */
#pragma once

#include "configuration.h"

#if !MESHTASTIC_EXCLUDE_MQTT && defined(TREKLINK_ONBOARD_QUEUE) && TREKLINK_ONBOARD_QUEUE
#define TREKLINK_OQ_ACTIVE 1
#else
#define TREKLINK_OQ_ACTIVE 0
#endif

#if TREKLINK_OQ_ACTIVE

#include "FSCommon.h"
#include "Observer.h"
#include "TrekLinkQueueConfig.h"
#include "TrekLinkQueueCore.h"
#include "mesh/generated/meshtastic/mesh.pb.h"

#include <string>
#include <vector>

class SafeFile;

/// LogStorage over LittleFS: appends with FSCom, compaction and meta through SafeFile (design.md section 1.2).
class TrekLinkFsLogStorage : public treklink::oq::LogStorage
{
  public:
    TrekLinkFsLogStorage();
    size_t size() override;
    bool append(const uint8_t *data, size_t len) override;
    bool read(size_t offset, uint8_t *buf, size_t len) override;
    bool beginRewrite() override;
    bool rewriteWrite(const uint8_t *data, size_t len) override;
    bool endRewrite() override;
    void abortRewrite() override;
    bool saveMeta(const uint8_t *data, size_t len) override;
    size_t loadMeta(uint8_t *buf, size_t cap) override;

    /// Free LittleFS bytes plus what the log already occupies, less the configured reserve.
    uint32_t availableForLog();

  private:
    size_t cachedSize;
    bool sizeKnown;
    File reader;
    bool readerOpen;
    SafeFile *rewriter;

    void closeReader();
    void refreshSize();
};

class TrekLinkEventQueue
{
  public:
    TrekLinkEventQueue();

    /// The core, restored from flash on first use (REQ-EVT-08). The MQTT thread only runs after fsInit().
    treklink::oq::QueueCore &core();

    bool empty() { return core().empty(); }

    /// Classify and enqueue an envelope already encoded by MQTT::onSend() (REQ-EVT-02 to REQ-EVT-04).
    void enqueue(const meshtastic_MeshPacket &mpDecoded, const std::string &topic, const uint8_t *env, size_t envLen);

    /// Health payload for the PRIVATE_APP packet (design.md section 2.4).
    void healthPayload(uint32_t uptimeS, std::vector<uint8_t> &out) { treklink::oq::encodeHealth(core().health(uptimeS), out); }

    /// True when a report should go out now: after an outage, or on the interval if anything changed.
    bool healthDue(uint32_t nowMs);
    void healthSent(uint32_t nowMs);
    void linkLost() { outageSinceReport = true; }

    /// Drain pacing (REQ-STA-03).
    uint32_t lastDrainMs;

  private:
    TrekLinkFsLogStorage storage;
    treklink::oq::QueueCore q;
    bool ready;
    bool outageSinceReport;
    uint32_t lastHealthMs;
    uint32_t lastHealthFingerprint;

    CallbackObserver<TrekLinkEventQueue, void *> rebootObserver =
        CallbackObserver<TrekLinkEventQueue, void *>(this, &TrekLinkEventQueue::persist);
    CallbackObserver<TrekLinkEventQueue, void *> deepSleepObserver =
        CallbackObserver<TrekLinkEventQueue, void *>(this, &TrekLinkEventQueue::persist);

    int persist(void *unused); // REQ-EVT-07
    uint32_t fingerprint();
    static bool localEpisodeActive();
};

#endif // TREKLINK_OQ_ACTIVE
