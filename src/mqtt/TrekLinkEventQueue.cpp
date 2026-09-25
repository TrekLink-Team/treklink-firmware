/*
 * TrekLink onboard-queue firmware adapter. See TrekLinkEventQueue.h and specs/onboard-queue/design.md.
 */
#include "TrekLinkEventQueue.h"

#if TREKLINK_OQ_ACTIVE

#include "MeshTypes.h"
#include "SPILock.h"
#include "SafeFile.h"
#include "TrekLinkQueueConfig.h"
#include "modules/FallDetectionModule.h"
#include "modules/TrekLinkButtonModule.h"
#include "modules/TrekLinkSOSGesture.h"
#include "modules/TrekLinkSOSHelper.h"
#include "sleep.h"

using namespace treklink::oq;

// The core keeps its own copies of these wire values so that it builds without the platform.
static_assert(PORTNUM_TEXT_MESSAGE_APP == meshtastic_PortNum_TEXT_MESSAGE_APP, "PortNum drift");
static_assert(PORTNUM_POSITION_APP == meshtastic_PortNum_POSITION_APP, "PortNum drift");
static_assert(PRIORITY_MAX == meshtastic_MeshPacket_Priority_MAX, "Priority drift");

static const char *OQ_DIR = "/treklink";
static const char *OQ_LOG = "/treklink/q.log";
static const char *OQ_META = "/treklink/q.meta";

// ---------------------------------------------------------------------------
// TrekLinkFsLogStorage
// ---------------------------------------------------------------------------

TrekLinkFsLogStorage::TrekLinkFsLogStorage() : cachedSize(0), sizeKnown(false), readerOpen(false), rewriter(NULL) {}

void TrekLinkFsLogStorage::closeReader()
{
    if (readerOpen) {
        reader.close();
        readerOpen = false;
    }
}

void TrekLinkFsLogStorage::refreshSize()
{
    concurrency::LockGuard g(spiLock);
    cachedSize = 0;
    File f = FSCom.open(OQ_LOG, FILE_O_READ);
    if (f) {
        cachedSize = f.size();
        f.close();
    }
    sizeKnown = true;
}

size_t TrekLinkFsLogStorage::size()
{
    if (!sizeKnown)
        refreshSize();
    return cachedSize;
}

bool TrekLinkFsLogStorage::append(const uint8_t *data, size_t len)
{
    closeReader();
    size_t written = 0;
    {
        concurrency::LockGuard g(spiLock);
        FSCom.mkdir(OQ_DIR);
        File f = FSCom.open(OQ_LOG, "a");
        if (f) {
            written = f.write(data, len);
            f.flush();
            f.close();
        }
    }
    if (written != len) {
        // Learn how much actually landed, so the core can detect a torn tail.
        refreshSize();
        return false;
    }
    cachedSize += len;
    return true;
}

bool TrekLinkFsLogStorage::read(size_t offset, uint8_t *buf, size_t len)
{
    concurrency::LockGuard g(spiLock);
    if (!readerOpen) {
        reader = FSCom.open(OQ_LOG, FILE_O_READ);
        readerOpen = (bool)reader;
        if (!readerOpen)
            return false;
    }
    if (!reader.seek(offset))
        return false;
    return reader.read(buf, len) == len;
}

bool TrekLinkFsLogStorage::beginRewrite()
{
    abortRewrite();
    {
        concurrency::LockGuard g(spiLock);
        FSCom.mkdir(OQ_DIR);
    }
    // fullAtomic: written to a temporary file and renamed over the log on close (SafeFile.cpp:79).
    rewriter = new SafeFile(OQ_LOG, true);
    return rewriter != NULL;
}

bool TrekLinkFsLogStorage::rewriteWrite(const uint8_t *data, size_t len)
{
    if (!rewriter)
        return false;
    concurrency::LockGuard g(spiLock);
    return rewriter->write(data, len) == len;
}

bool TrekLinkFsLogStorage::endRewrite()
{
    if (!rewriter)
        return false;
    closeReader();
    bool ok = rewriter->close();
    delete rewriter;
    rewriter = NULL;
    refreshSize();
    return ok;
}

void TrekLinkFsLogStorage::abortRewrite()
{
    // Dropping the SafeFile without close() leaves the old log in place; only the temporary file is abandoned.
    if (rewriter) {
        delete rewriter;
        rewriter = NULL;
    }
}

bool TrekLinkFsLogStorage::saveMeta(const uint8_t *data, size_t len)
{
    {
        concurrency::LockGuard g(spiLock);
        FSCom.mkdir(OQ_DIR);
    }
    SafeFile f(OQ_META, true);
    {
        concurrency::LockGuard g(spiLock);
        f.write(data, len);
    }
    return f.close();
}

size_t TrekLinkFsLogStorage::loadMeta(uint8_t *buf, size_t cap)
{
    concurrency::LockGuard g(spiLock);
    File f = FSCom.open(OQ_META, FILE_O_READ);
    if (!f)
        return 0;
    size_t n = 0;
    if (f.size() <= cap)
        n = f.read(buf, cap);
    f.close();
    return n;
}

uint32_t TrekLinkFsLogStorage::availableForLog()
{
    size_t log = size();
    concurrency::LockGuard g(spiLock);
    size_t total = FSCom.totalBytes();
    size_t used = FSCom.usedBytes();
    size_t avail = (total > used ? total - used : 0) + log;
    return avail > TREKLINK_OQ_FS_RESERVE_BYTES ? (uint32_t)(avail - TREKLINK_OQ_FS_RESERVE_BYTES) : 0;
}

// ---------------------------------------------------------------------------
// TrekLinkEventQueue
// ---------------------------------------------------------------------------

TrekLinkEventQueue::TrekLinkEventQueue()
    : lastDrainMs(0), q(defaultConfig(), &storage), ready(false), outageSinceReport(false), lastHealthMs(0),
      lastHealthFingerprint(0)
{
    rebootObserver.observe(&notifyReboot);
    deepSleepObserver.observe(&notifyDeepSleep);
}

QueueCore &TrekLinkEventQueue::core()
{
    if (!ready) {
        ready = true;
        // REQ-ERR-06: never accept a budget the filesystem cannot hold.
        uint32_t avail = storage.availableForLog();
        if (avail < q.config().flashBudgetBytes) {
            LOG_WARN("TrekLinkQueue: flash budget %u clamped to %u bytes of free LittleFS", (unsigned)q.config().flashBudgetBytes,
                     (unsigned)avail);
            q.setFlashBudget(avail);
        }
        q.restore();
        LOG_INFO("TrekLinkQueue: restored %u entries (P0 %u, P1 %u, P2 %u, P3 %u), log %u bytes, discarded %u",
                 (unsigned)q.depth(), (unsigned)q.depth(TIER_P0), (unsigned)q.depth(TIER_P1), (unsigned)q.depth(TIER_P2),
                 (unsigned)q.depth(TIER_P3), (unsigned)q.flashBytes(), (unsigned)q.stats().restoreDiscarded);
    }
    return q;
}

bool TrekLinkEventQueue::localEpisodeActive()
{
    bool active = false;
#ifdef TREKLINK_VARIANT
#ifdef BUTTON_PIN_SOS
    active = active || (trekLinkButtonModule && trekLinkButtonModule->isSOSActive());
#else
    active = active || (trekLinkSOSGesture && trekLinkSOSGesture->isSOSActive());
#endif
#endif
    active = active || (fallDetectionModule && fallDetectionModule->isInSOSTriggered());
    return active;
}

void TrekLinkEventQueue::enqueue(const meshtastic_MeshPacket &mp, const std::string &topic, const uint8_t *env, size_t envLen)
{
    QueueCore &c = core();
    bool decoded = mp.which_payload_variant == meshtastic_MeshPacket_decoded_tag;
    bool fromUs = isFromUs(&mp);
    bool episode = fromUs && localEpisodeActive();

    ClassifyInput in;
    in.decoded = decoded;
    in.portnum = decoded ? (uint32_t)mp.decoded.portnum : 0;
    in.priority = (uint8_t)mp.priority;
    in.fromUs = fromUs;
    in.episodeActive = episode;
    in.payload = decoded ? mp.decoded.payload.bytes : NULL;
    in.payloadLen = decoded ? mp.decoded.payload.size : 0;
    Tier tier = classify(in, TREKLINK_SOS_TEXT_PREFIX);

    c.setEpisodeActive(localEpisodeActive());
    EnqueueResult r = c.enqueue(tier, episode && tier == TIER_P1, mp.id, topic, env, envLen);
    switch (r) {
    case ENQ_ACCEPTED:
        LOG_INFO("TrekLinkQueue: queued id=0x%08x tier P%u, depth %u", (unsigned)mp.id, (unsigned)tier, (unsigned)c.depth());
        break;
    case ENQ_ACCEPTED_AFTER_SHED:
        LOG_WARN("TrekLinkQueue: full, shed one entry to queue id=0x%08x tier P%u", (unsigned)mp.id, (unsigned)tier);
        break;
    case ENQ_REFUSED:
        LOG_WARN("TrekLinkQueue: full, shed incoming id=0x%08x tier P%u", (unsigned)mp.id, (unsigned)tier);
        break;
    case ENQ_REFUSED_P0:
        LOG_ERROR("TrekLinkQueue: full of unsheddable entries, refused SOS id=0x%08x", (unsigned)mp.id);
        break;
    }
}

uint32_t TrekLinkEventQueue::fingerprint()
{
    const Stats &s = core().stats();
    uint32_t f = (uint32_t)q.depth() * 2654435761u;
    for (uint8_t t = 0; t < TIER_COUNT; t++)
        f = f * 31u + s.enqueued[t] * 7u + s.published[t] * 13u + s.shed[t] * 17u;
    return f + s.p0Refused * 19u + s.flashWriteFailed * 23u + s.restoreDiscarded * 29u;
}

bool TrekLinkEventQueue::healthDue(uint32_t nowMs)
{
    if (outageSinceReport)
        return true;
    if (lastHealthMs != 0 && nowMs - lastHealthMs < TREKLINK_OQ_HEALTH_INTERVAL_S * 1000UL)
        return false;
    // Suppressed while nothing changed, which covers an empty queue on a healthy link (Q1).
    return fingerprint() != lastHealthFingerprint;
}

void TrekLinkEventQueue::healthSent(uint32_t nowMs)
{
    outageSinceReport = false;
    lastHealthMs = nowMs ? nowMs : 1;
    lastHealthFingerprint = fingerprint();
    core().saveMeta(); // REQ-UBI-06: counters persist at every health report
}

int TrekLinkEventQueue::persist(void *)
{
    if (ready) {
        LOG_INFO("TrekLinkQueue: persisting %u entries before reboot or sleep", (unsigned)q.depth());
        q.persistAll();
    }
    return 0;
}

#endif // TREKLINK_OQ_ACTIVE
