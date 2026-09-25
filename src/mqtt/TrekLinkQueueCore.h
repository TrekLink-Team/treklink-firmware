/*
 * TrekLink onboard-queue policy core (specs/onboard-queue/design.md sections 1 and 2).
 *
 * A priority-ordered, shed-aware, flash-backed outbound queue for MQTT envelopes.
 * This layer is platform-free: it uses only the C++11 standard library and reaches
 * flash through the LogStorage interface, so every rule that carries an NFR is unit
 * tested on a host without a device. The firmware adapter (TrekLinkEventQueue) wires
 * it to MQTT, LittleFS and the SOS modules.
 *
 * Nothing here has global state or a static initialiser; unreferenced, it is discarded
 * at link time.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace treklink
{
namespace oq
{

/// Priority tiers of MF-02. Lower value drains first and is shed last.
enum Tier : uint8_t { TIER_P0 = 0, TIER_P1 = 1, TIER_P2 = 2, TIER_P3 = 3 };
static const uint8_t TIER_COUNT = 4;

/// Wire values the classifier needs. The adapter static_asserts them against the generated protobuf enums.
static const uint32_t PORTNUM_TEXT_MESSAGE_APP = 1;
static const uint32_t PORTNUM_POSITION_APP = 3;
static const uint8_t PRIORITY_MAX = 127;

/// Everything classify() looks at, lifted out of a MeshPacket by the adapter.
struct ClassifyInput {
    bool decoded;       // payload is decoded (not an encrypted blob)
    uint32_t portnum;   // decoded.portnum
    uint8_t priority;   // MeshPacket.priority after fixPriority()
    bool fromUs;        // originated by this node
    bool episodeActive; // an SOS episode is active on this node
    const uint8_t *payload;
    size_t payloadLen;
};

/// REQ-EVT-03. sosPrefix is the one definition from TrekLinkSOSHelper.h.
Tier classify(const ClassifyInput &in, const char *sosPrefix);

/// Behavioural parameters, filled from TrekLinkQueueConfig.h by the adapter.
struct Config {
    uint16_t maxEntries;
    uint16_t ramEntries;
    uint16_t spillBatch; // 0 means ramEntries / 4, at least 1
    uint32_t flashBudgetBytes;
    uint8_t episodeKeepFirst;
    uint8_t episodeKeepLatest;
    uint16_t tombstoneBatch;
    uint32_t compactMinBytes;
    uint8_t compactDeadPercent;
    uint16_t maxRecordBytes;
};

/// Config populated from the build-time constants.
Config defaultConfig();

/// REQ-UBI-06 counters. Monotonic; persisted with the meta record.
struct Stats {
    uint32_t enqueued[TIER_COUNT];
    uint32_t published[TIER_COUNT];
    uint32_t shed[TIER_COUNT];
    uint32_t p0Refused;
    uint32_t flashWriteFailed;
    uint32_t restoreDiscarded; // bytes
};

/// Flash access used by the core. Implemented over FSCom on device and over memory in tests.
class LogStorage
{
  public:
    virtual ~LogStorage() {}
    virtual size_t size() = 0;
    /// Append and flush. On failure the log must be unchanged or end in a torn record that restore() rejects.
    virtual bool append(const uint8_t *data, size_t len) = 0;
    virtual bool read(size_t offset, uint8_t *buf, size_t len) = 0;
    /// Compaction: stream a replacement log, then swap it in atomically.
    virtual bool beginRewrite() = 0;
    virtual bool rewriteWrite(const uint8_t *data, size_t len) = 0;
    virtual bool endRewrite() = 0;
    virtual void abortRewrite() {}
    virtual bool saveMeta(const uint8_t *data, size_t len) = 0;
    virtual size_t loadMeta(uint8_t *buf, size_t cap) = 0;
};

enum EnqueueResult : uint8_t {
    ENQ_ACCEPTED = 0,
    ENQ_ACCEPTED_AFTER_SHED = 1, // a queued entry was shed to make room
    ENQ_REFUSED = 2,             // incoming non-P0 was the shed victim
    ENQ_REFUSED_P0 = 3,          // incoming P0 and nothing sheddable (REQ-ERR-01)
};

/// The next entry to publish, with its body loaded.
struct Pending {
    uint32_t seq;
    uint32_t packetId;
    Tier tier;
    std::string topic;
    std::vector<uint8_t> env;
};

// ---- Record codec (design.md section 1.1) ----

static const uint8_t RECORD_MAGIC = 0xA7;
static const uint8_t RECORD_DATA = 1;
static const uint8_t RECORD_TOMBSTONE = 2;
static const size_t RECORD_HEADER_BYTES = 4;  // magic, type, bodyLen
static const size_t RECORD_TRAILER_BYTES = 4; // crc32
static const uint8_t FLAG_EPISODE = 0x01;

uint32_t crc32(const uint8_t *data, size_t len, uint32_t seed = 0);

/// Appends one DATA record to out.
void encodeData(std::vector<uint8_t> &out, uint32_t seq, uint32_t packetId, Tier tier, uint8_t flags, const std::string &topic,
                const uint8_t *env, size_t envLen);

/// Appends one TOMBSTONE record per 0xFFFF seqs to out.
void encodeTombstones(std::vector<uint8_t> &out, const std::vector<uint32_t> &seqs);

struct DataRecord {
    uint32_t seq;
    uint32_t packetId;
    Tier tier;
    uint8_t flags;
    std::string topic;
    std::vector<uint8_t> env;
};

/// Decodes a complete DATA record (header, body and CRC). Returns false on any inconsistency.
bool decodeData(const uint8_t *rec, size_t len, DataRecord &out);

// ---- The queue ----

class QueueCore
{
  public:
    /// storage may be null: the queue then runs RAM-only with ramEntries as its bound.
    QueueCore(const Config &cfg, LogStorage *storage);

    /// REQ-EVT-08, REQ-ERR-03. Call once, before anything else.
    void restore();

    /// REQ-EVT-02 to REQ-EVT-06, REQ-STA-01 to REQ-STA-05, REQ-ERR-01, REQ-ERR-02.
    EnqueueResult enqueue(Tier tier, bool episode, uint32_t packetId, const std::string &topic, const uint8_t *env,
                          size_t envLen);

    /// REQ-EVT-09: lowest tier, then lowest seq, skipping nothing. False when empty or unreadable.
    bool peek(Pending &out);

    /// The entry being published; it cannot be shed until commit() or release().
    void markInFlight(uint32_t seq);
    /// REQ-EVT-10: publish confirmed, remove the entry.
    void commit(uint32_t seq);
    /// Publish not confirmed, keep the entry queued.
    void release(uint32_t seq);
    bool hasInFlight() const { return inFlightSeq != NO_SEQ; }
    uint32_t inFlight() const { return inFlightSeq; }

    /// REQ-EVT-07: spill every RAM entry, flush tombstones, save counters.
    void persistAll();

    /// REQ-STA-04 is enforced only while an episode is active.
    void setEpisodeActive(bool active) { episodeActive = active; }

    /// Lower the flash budget, for example after measuring free space (REQ-ERR-06).
    void setFlashBudget(uint32_t bytes) { cfg.flashBudgetBytes = bytes; }

    size_t depth() const { return entries.size(); }
    size_t depth(Tier tier) const;
    size_t ramResident() const;
    bool empty() const { return entries.empty(); }
    /// Effective total bound right now (REQ-STA-05).
    size_t capacity() const;
    bool flashHealthy() const { return storage != NULL && flashOk; }
    size_t flashBytes() const { return storage ? storage->size() : 0; }
    const Config &config() const { return cfg; }
    const Stats &stats() const { return st; }

    /// design.md section 2.4 payload.
    std::string healthJson(uint32_t uptimeS) const;

    /// Save counters and nextSeq only.
    void saveMeta();

  private:
    static const uint32_t NO_SEQ = 0xFFFFFFFFu;
    static const uint32_t NOT_ON_FLASH = 0xFFFFFFFFu;

    struct Entry {
        uint32_t seq;
        uint32_t packetId;
        uint32_t offset; // NOT_ON_FLASH when the record is held in rec
        uint16_t len;    // full record length
        uint8_t tier;
        uint8_t flags;
        std::vector<uint8_t> rec; // encoded DATA record while RAM-resident
    };

    Config cfg;
    LogStorage *storage;
    std::vector<Entry> entries;
    std::vector<uint32_t> pendingTombstones;
    Stats st;
    uint32_t nextSeq;
    uint32_t inFlightSeq;
    uint32_t liveFlashBytes;
    bool flashOk;
    bool episodeActive;
    bool needCompact; // a torn append or corrupt tail must be cut before the next append

    bool isExempt(size_t idx, const std::vector<uint32_t> &kept) const;
    void keptEpisodeSeqs(std::vector<uint32_t> &out, uint32_t extraSeq) const;
    int pickVictim(const std::vector<uint32_t> &kept) const;
    void shedAt(size_t idx);
    int findIndex(uint32_t seq) const;
    void removeAt(size_t idx);
    bool writeFlash(const std::vector<uint8_t> &buf, uint32_t &bufOffset); // prepends pending tombstones
    bool writeThrough(Entry &e);
    bool rescueSos();
    bool spillIfNeeded(bool all);
    void flushTombstones(bool force);
    void compactIfNeeded(size_t incomingBytes);
    bool compact();
    bool loadRecord(const Entry &e, std::vector<uint8_t> &rec);
};

} // namespace oq
} // namespace treklink
