/*
 * TrekLink onboard-queue policy core. See TrekLinkQueueCore.h and specs/onboard-queue/design.md.
 */
#include "TrekLinkQueueCore.h"
#include "TrekLinkQueueConfig.h"

#include <algorithm>
#include <string.h>

namespace treklink
{
namespace oq
{

// ---------------------------------------------------------------------------
// Classification (design.md section 2.1)
// ---------------------------------------------------------------------------

static bool startsWith(const uint8_t *data, size_t len, const char *prefix)
{
    if (prefix == NULL || *prefix == '\0' || data == NULL)
        return false;
    size_t plen = strlen(prefix);
    return len >= plen && memcmp(data, prefix, plen) == 0;
}

Tier classify(const ClassifyInput &in, const char *sosPrefix)
{
    if (!in.decoded)
        return TIER_P3;
    if (in.portnum == PORTNUM_TEXT_MESSAGE_APP) {
        // A peer's SOS arrives with priority HIGH (fixPriority), so the prefix is the only signal that survives the hop.
        if (startsWith(in.payload, in.payloadLen, sosPrefix))
            return TIER_P0;
        if (in.fromUs && in.priority == PRIORITY_MAX)
            return TIER_P0;
        return TIER_P3;
    }
    if (in.portnum == PORTNUM_POSITION_APP) {
        // MAX catches the trigger-time position, sent before the SOS modules set their flag.
        if (in.fromUs && (in.episodeActive || in.priority == PRIORITY_MAX))
            return TIER_P1;
        return TIER_P2;
    }
    return TIER_P3;
}

Config defaultConfig()
{
    Config c;
    c.maxEntries = TREKLINK_OQ_MAX_ENTRIES;
    c.ramEntries = TREKLINK_OQ_RAM_ENTRIES;
    c.spillBatch = TREKLINK_OQ_SPILL_BATCH;
    c.flashBudgetBytes = TREKLINK_OQ_FLASH_BUDGET_BYTES;
    c.episodeKeepFirst = TREKLINK_OQ_EPISODE_KEEP_FIRST;
    c.episodeKeepLatest = TREKLINK_OQ_EPISODE_KEEP_LATEST;
    c.tombstoneBatch = TREKLINK_OQ_TOMBSTONE_BATCH;
    c.compactMinBytes = TREKLINK_OQ_COMPACT_MIN_BYTES;
    c.compactDeadPercent = TREKLINK_OQ_COMPACT_DEAD_PERCENT;
    c.maxRecordBytes = TREKLINK_OQ_MAX_RECORD_BYTES;
    return c;
}

// ---------------------------------------------------------------------------
// Record codec (design.md section 1.1)
// ---------------------------------------------------------------------------

uint32_t crc32(const uint8_t *data, size_t len, uint32_t seed)
{
    uint32_t crc = ~seed;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

static void put16(std::vector<uint8_t> &o, uint16_t v)
{
    o.push_back((uint8_t)(v & 0xFF));
    o.push_back((uint8_t)(v >> 8));
}

static void put32(std::vector<uint8_t> &o, uint32_t v)
{
    for (int i = 0; i < 4; i++)
        o.push_back((uint8_t)(v >> (8 * i)));
}

static uint16_t get16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// Header (magic, type, bodyLen) is written first; bodyLen and the CRC are patched in once the body is known.
static size_t beginRecord(std::vector<uint8_t> &out, uint8_t type)
{
    size_t start = out.size();
    out.push_back(RECORD_MAGIC);
    out.push_back(type);
    put16(out, 0);
    return start;
}

static void endRecord(std::vector<uint8_t> &out, size_t start)
{
    size_t bodyLen = out.size() - start - RECORD_HEADER_BYTES;
    out[start + 2] = (uint8_t)(bodyLen & 0xFF);
    out[start + 3] = (uint8_t)(bodyLen >> 8);
    // CRC covers type, bodyLen and body: everything after the magic byte.
    put32(out, crc32(&out[start + 1], out.size() - start - 1));
}

void encodeData(std::vector<uint8_t> &out, uint32_t seq, uint32_t packetId, Tier tier, uint8_t flags, const std::string &topic,
                const uint8_t *env, size_t envLen)
{
    size_t start = beginRecord(out, RECORD_DATA);
    put32(out, seq);
    put32(out, packetId);
    out.push_back((uint8_t)tier);
    out.push_back(flags);
    put16(out, (uint16_t)topic.size());
    out.insert(out.end(), topic.begin(), topic.end());
    if (envLen)
        out.insert(out.end(), env, env + envLen);
    endRecord(out, start);
}

void encodeTombstones(std::vector<uint8_t> &out, const std::vector<uint32_t> &seqs)
{
    // 250 seqs keep one tombstone body (2 + 4 * 250 bytes) under the default record limit.
    const size_t perRecord = 250;
    for (size_t i = 0; i < seqs.size(); i += perRecord) {
        size_t n = std::min(perRecord, seqs.size() - i);
        size_t start = beginRecord(out, RECORD_TOMBSTONE);
        put16(out, (uint16_t)n);
        for (size_t k = 0; k < n; k++)
            put32(out, seqs[i + k]);
        endRecord(out, start);
    }
}

// Validates frame and CRC. Returns the body length, or -1.
static long checkFrame(const uint8_t *rec, size_t len)
{
    if (len < RECORD_HEADER_BYTES + RECORD_TRAILER_BYTES || rec[0] != RECORD_MAGIC)
        return -1;
    size_t bodyLen = get16(rec + 2);
    if (RECORD_HEADER_BYTES + bodyLen + RECORD_TRAILER_BYTES != len)
        return -1;
    if (crc32(rec + 1, RECORD_HEADER_BYTES - 1 + bodyLen) != get32(rec + RECORD_HEADER_BYTES + bodyLen))
        return -1;
    return (long)bodyLen;
}

bool decodeData(const uint8_t *rec, size_t len, DataRecord &out)
{
    long bodyLen = checkFrame(rec, len);
    if (bodyLen < 12 || rec[1] != RECORD_DATA)
        return false;
    const uint8_t *b = rec + RECORD_HEADER_BYTES;
    out.seq = get32(b);
    out.packetId = get32(b + 4);
    if (b[8] >= TIER_COUNT)
        return false;
    out.tier = (Tier)b[8];
    out.flags = b[9];
    size_t topicLen = get16(b + 10);
    if (12 + topicLen > (size_t)bodyLen)
        return false;
    out.topic.assign((const char *)b + 12, topicLen);
    out.env.assign(b + 12 + topicLen, b + bodyLen);
    return true;
}

// ---------------------------------------------------------------------------
// Meta record: counters and nextSeq
// ---------------------------------------------------------------------------

static const uint8_t META_MAGIC[4] = {'T', 'K', 'Q', 'M'};
static const uint8_t META_VERSION = 1;
static const size_t META_WORDS = 1 + 3 * TIER_COUNT + 3; // nextSeq, three per-tier arrays, three scalars
static const size_t META_BYTES = 4 + 1 + 4 * META_WORDS + 4;

// ---------------------------------------------------------------------------
// QueueCore
// ---------------------------------------------------------------------------

QueueCore::QueueCore(const Config &c, LogStorage *s)
    : cfg(c), storage(s), nextSeq(0), inFlightSeq(NO_SEQ), liveFlashBytes(0), flashOk(s != NULL), episodeActive(false),
      needCompact(false)
{
    memset(&st, 0, sizeof(st));
    if (cfg.ramEntries == 0)
        cfg.ramEntries = 1;
    if (cfg.maxEntries < cfg.ramEntries)
        cfg.maxEntries = cfg.ramEntries;
    if (cfg.spillBatch == 0)
        cfg.spillBatch = std::max<uint16_t>(1, cfg.ramEntries / 4);
    // The episode exemption must leave at least one entry sheddable at the smallest bound (REQ-STA-04, REQ-ERR-01).
    while (cfg.episodeKeepFirst + cfg.episodeKeepLatest >= cfg.ramEntries &&
           (cfg.episodeKeepFirst > 0 || cfg.episodeKeepLatest > 0)) {
        if (cfg.episodeKeepFirst >= cfg.episodeKeepLatest && cfg.episodeKeepFirst > 0)
            cfg.episodeKeepFirst--;
        else
            cfg.episodeKeepLatest--;
    }
}

size_t QueueCore::depth(Tier tier) const
{
    size_t n = 0;
    for (size_t i = 0; i < entries.size(); i++)
        if (entries[i].tier == tier)
            n++;
    return n;
}

size_t QueueCore::ramResident() const
{
    size_t n = 0;
    for (size_t i = 0; i < entries.size(); i++)
        if (entries[i].offset == NOT_ON_FLASH)
            n++;
    return n;
}

size_t QueueCore::capacity() const
{
    return flashHealthy() ? cfg.maxEntries : cfg.ramEntries;
}

int QueueCore::findIndex(uint32_t seq) const
{
    for (size_t i = 0; i < entries.size(); i++)
        if (entries[i].seq == seq)
            return (int)i;
    return -1;
}

void QueueCore::removeAt(size_t idx)
{
    Entry &e = entries[idx];
    if (e.offset != NOT_ON_FLASH) {
        pendingTombstones.push_back(e.seq);
        liveFlashBytes -= e.len;
    }
    if (e.seq == inFlightSeq)
        inFlightSeq = NO_SEQ;
    if (idx != entries.size() - 1)
        std::swap(entries[idx], entries.back());
    entries.pop_back();
}

// ---- Shedding (design.md section 2.2) ----

void QueueCore::keptEpisodeSeqs(std::vector<uint32_t> &out, uint32_t extraSeq) const
{
    out.clear();
    if (!episodeActive)
        return;
    std::vector<uint32_t> all;
    for (size_t i = 0; i < entries.size(); i++)
        if (entries[i].flags & FLAG_EPISODE)
            all.push_back(entries[i].seq);
    if (extraSeq != NO_SEQ)
        all.push_back(extraSeq);
    std::sort(all.begin(), all.end());
    for (size_t i = 0; i < all.size(); i++)
        if (i < cfg.episodeKeepFirst || i + cfg.episodeKeepLatest >= all.size())
            out.push_back(all[i]);
}

static bool contains(const std::vector<uint32_t> &v, uint32_t x)
{
    return std::find(v.begin(), v.end(), x) != v.end();
}

bool QueueCore::isExempt(size_t idx, const std::vector<uint32_t> &kept) const
{
    const Entry &e = entries[idx];
    return e.tier == TIER_P0 || e.seq == inFlightSeq || contains(kept, e.seq);
}

// Newest sheddable queued entry in the lowest-priority occupied tier, or -1.
int QueueCore::pickVictim(const std::vector<uint32_t> &kept) const
{
    int best = -1;
    for (size_t i = 0; i < entries.size(); i++) {
        if (isExempt(i, kept))
            continue;
        if (best < 0 || entries[i].tier > entries[best].tier ||
            (entries[i].tier == entries[best].tier && entries[i].seq > entries[best].seq))
            best = (int)i;
    }
    return best;
}

void QueueCore::shedAt(size_t idx)
{
    st.shed[entries[idx].tier]++;
    removeAt(idx);
}

// ---- Flash writes ----

bool QueueCore::writeFlash(const std::vector<uint8_t> &buf, uint32_t &bufOffset)
{
    if (!storage)
        return false;
    if (needCompact && !compact())
        return false;

    std::vector<uint8_t> out;
    encodeTombstones(out, pendingTombstones);
    size_t tombBytes = out.size();
    out.insert(out.end(), buf.begin(), buf.end());
    if (out.empty())
        return true;

    compactIfNeeded(out.size());
    if (storage->size() + out.size() > cfg.flashBudgetBytes) {
        flashOk = false;
        return false;
    }

    size_t base = storage->size();
    if (!storage->append(&out[0], out.size())) {
        flashOk = false;
        // A torn tail would hide every later append at restore; rewrite before writing again.
        if (storage->size() != base)
            needCompact = true;
        return false;
    }
    pendingTombstones.clear();
    flashOk = true;
    bufOffset = (uint32_t)(base + tombBytes);
    return true;
}

bool QueueCore::writeThrough(Entry &e)
{
    uint32_t off = 0;
    if (!writeFlash(e.rec, off)) {
        st.flashWriteFailed++;
        return false;
    }
    e.offset = off;
    liveFlashBytes += e.len;
    std::vector<uint8_t>().swap(e.rec);
    return true;
}

static bool spillOrder(const std::pair<uint8_t, uint32_t> &a, const std::pair<uint8_t, uint32_t> &b)
{
    // Lowest priority first, oldest first within a tier.
    if (a.first != b.first)
        return a.first > b.first;
    return a.second < b.second;
}

bool QueueCore::rescueSos()
{
    // P0 and P1 held in RAM because an earlier write failed go to flash first (REQ-EVT-06).
    std::vector<uint8_t> buf;
    std::vector<size_t> idx;
    for (size_t i = 0; i < entries.size(); i++)
        if (entries[i].offset == NOT_ON_FLASH && entries[i].tier <= TIER_P1) {
            idx.push_back(i);
            buf.insert(buf.end(), entries[i].rec.begin(), entries[i].rec.end());
        }
    if (idx.empty())
        return true;
    uint32_t off = 0;
    if (!writeFlash(buf, off)) {
        st.flashWriteFailed++;
        return false;
    }
    for (size_t k = 0; k < idx.size(); k++) {
        Entry &e = entries[idx[k]];
        e.offset = off;
        off += e.len;
        liveFlashBytes += e.len;
        std::vector<uint8_t>().swap(e.rec);
    }
    return true;
}

bool QueueCore::spillIfNeeded(bool all)
{
    if (!storage)
        return true;
    if (flashOk && !rescueSos())
        return false;
    size_t ram = ramResident();
    if (ram == 0 || (!all && ram <= cfg.ramEntries))
        return true;

    size_t n = all ? ram : std::max<size_t>(cfg.spillBatch, ram - cfg.ramEntries);
    std::vector<std::pair<uint8_t, uint32_t>> order;
    for (size_t i = 0; i < entries.size(); i++)
        if (entries[i].offset == NOT_ON_FLASH)
            order.push_back(std::make_pair(entries[i].tier, entries[i].seq));
    std::sort(order.begin(), order.end(), spillOrder);
    if (n > order.size())
        n = order.size();

    std::vector<uint8_t> buf;
    std::vector<size_t> idx;
    for (size_t k = 0; k < n; k++) {
        int i = findIndex(order[k].second);
        idx.push_back((size_t)i);
        buf.insert(buf.end(), entries[i].rec.begin(), entries[i].rec.end());
    }
    uint32_t off = 0;
    if (!writeFlash(buf, off)) {
        st.flashWriteFailed++;
        return false;
    }
    for (size_t k = 0; k < idx.size(); k++) {
        Entry &e = entries[idx[k]];
        e.offset = off;
        off += e.len;
        liveFlashBytes += e.len;
        std::vector<uint8_t>().swap(e.rec);
    }
    return true;
}

void QueueCore::flushTombstones(bool force)
{
    if (pendingTombstones.empty() || !storage)
        return;
    if (!force && pendingTombstones.size() < cfg.tombstoneBatch)
        return;
    uint32_t off;
    std::vector<uint8_t> none;
    if (!writeFlash(none, off))
        st.flashWriteFailed++;
}

void QueueCore::compactIfNeeded(size_t incomingBytes)
{
    if (!storage)
        return;
    size_t size = storage->size();
    size_t dead = size > liveFlashBytes ? size - liveFlashBytes : 0;
    bool share = size >= cfg.compactMinBytes && dead * 100 >= (size_t)cfg.compactDeadPercent * size;
    bool budget = dead > 0 && size + incomingBytes > cfg.flashBudgetBytes;
    if (share || budget)
        compact();
}

bool QueueCore::compact()
{
    if (!storage)
        return false;
    // Stream flash-resident records in log order into a fresh log.
    std::vector<std::pair<uint32_t, size_t>> order; // offset, entry index
    for (size_t i = 0; i < entries.size(); i++)
        if (entries[i].offset != NOT_ON_FLASH)
            order.push_back(std::make_pair(entries[i].offset, i));
    std::sort(order.begin(), order.end());

    if (!storage->beginRewrite())
        return false;
    std::vector<uint32_t> newOffsets(order.size());
    uint32_t pos = 0;
    std::vector<uint8_t> rec;
    for (size_t k = 0; k < order.size(); k++) {
        const Entry &e = entries[order[k].second];
        if (!loadRecord(e, rec) || !storage->rewriteWrite(&rec[0], rec.size())) {
            storage->abortRewrite();
            return false;
        }
        newOffsets[k] = pos;
        pos += e.len;
    }
    if (!storage->endRewrite())
        return false;
    for (size_t k = 0; k < order.size(); k++)
        entries[order[k].second].offset = newOffsets[k];
    // Dead records were not copied, so their tombstones are no longer needed.
    pendingTombstones.clear();
    liveFlashBytes = pos;
    needCompact = false;
    return true;
}

bool QueueCore::loadRecord(const Entry &e, std::vector<uint8_t> &rec)
{
    if (e.offset == NOT_ON_FLASH) {
        rec = e.rec;
        return true;
    }
    rec.resize(e.len);
    return storage && storage->read(e.offset, &rec[0], e.len);
}

// ---- Public operations ----

EnqueueResult QueueCore::enqueue(Tier tier, bool episode, uint32_t packetId, const std::string &topic, const uint8_t *env,
                                 size_t envLen)
{
    st.enqueued[tier]++;
    uint32_t seq = nextSeq++;

    if (12 + topic.size() + envLen > cfg.maxRecordBytes) {
        // restore() would treat a larger record as corruption, so it can never be stored.
        if (tier == TIER_P0)
            st.p0Refused++;
        else
            st.shed[tier]++;
        return tier == TIER_P0 ? ENQ_REFUSED_P0 : ENQ_REFUSED;
    }

    uint8_t flags = episode ? FLAG_EPISODE : 0;
    bool shedSome = false;
    std::vector<uint32_t> kept;
    while (entries.size() >= capacity()) {
        keptEpisodeSeqs(kept, episode ? seq : NO_SEQ);
        int victim = pickVictim(kept);
        bool incomingExempt = tier == TIER_P0 || contains(kept, seq);
        // The incoming entry is the newest of all, so it loses any tie within its tier.
        if (victim < 0 || (!incomingExempt && tier >= entries[victim].tier)) {
            if (tier == TIER_P0) {
                st.p0Refused++;
                return ENQ_REFUSED_P0;
            }
            st.shed[tier]++;
            return ENQ_REFUSED;
        }
        shedAt((size_t)victim);
        shedSome = true;
    }

    Entry e;
    e.seq = seq;
    e.packetId = packetId;
    e.offset = NOT_ON_FLASH;
    e.tier = tier;
    e.flags = flags;
    encodeData(e.rec, seq, packetId, tier, flags, topic, env, envLen);
    e.len = (uint16_t)e.rec.size();

    if (tier <= TIER_P1 && storage)
        writeThrough(e);
    entries.push_back(e);

    if (!spillIfNeeded(false)) {
        // Flash is failing: the bound is now the RAM bound (REQ-STA-05).
        while (entries.size() > capacity()) {
            keptEpisodeSeqs(kept, NO_SEQ);
            int victim = pickVictim(kept);
            if (victim < 0)
                break;
            bool wasIncoming = entries[victim].seq == seq;
            shedAt((size_t)victim);
            if (wasIncoming)
                return ENQ_REFUSED;
            shedSome = true;
        }
    }
    flushTombstones(false);
    return shedSome ? ENQ_ACCEPTED_AFTER_SHED : ENQ_ACCEPTED;
}

bool QueueCore::peek(Pending &out)
{
    while (!entries.empty()) {
        size_t best = 0;
        for (size_t i = 1; i < entries.size(); i++)
            if (entries[i].tier < entries[best].tier ||
                (entries[i].tier == entries[best].tier && entries[i].seq < entries[best].seq))
                best = i;
        std::vector<uint8_t> rec;
        DataRecord d;
        if (loadRecord(entries[best], rec) && decodeData(&rec[0], rec.size(), d) && d.seq == entries[best].seq) {
            out.seq = d.seq;
            out.packetId = d.packetId;
            out.tier = d.tier;
            out.topic.swap(d.topic);
            out.env.swap(d.env);
            return true;
        }
        // Unreadable after a successful restore: flash went bad under us. Count it and move on.
        shedAt(best);
    }
    return false;
}

void QueueCore::markInFlight(uint32_t seq)
{
    if (findIndex(seq) >= 0)
        inFlightSeq = seq;
}

void QueueCore::commit(uint32_t seq)
{
    int i = findIndex(seq);
    if (i >= 0) {
        st.published[entries[i].tier]++;
        removeAt((size_t)i);
    }
    if (inFlightSeq == seq)
        inFlightSeq = NO_SEQ;
    flushTombstones(false);
    compactIfNeeded(0);
}

void QueueCore::release(uint32_t seq)
{
    if (inFlightSeq == seq)
        inFlightSeq = NO_SEQ;
}

void QueueCore::persistAll()
{
    spillIfNeeded(true);
    flushTombstones(true);
    saveMeta();
}

void QueueCore::saveMeta()
{
    if (!storage)
        return;
    std::vector<uint8_t> b(META_MAGIC, META_MAGIC + 4);
    b.push_back(META_VERSION);
    put32(b, nextSeq);
    for (uint8_t t = 0; t < TIER_COUNT; t++)
        put32(b, st.enqueued[t]);
    for (uint8_t t = 0; t < TIER_COUNT; t++)
        put32(b, st.published[t]);
    for (uint8_t t = 0; t < TIER_COUNT; t++)
        put32(b, st.shed[t]);
    put32(b, st.p0Refused);
    put32(b, st.flashWriteFailed);
    put32(b, st.restoreDiscarded);
    put32(b, crc32(&b[0], b.size()));
    storage->saveMeta(&b[0], b.size());
}

void QueueCore::restore()
{
    if (!storage)
        return;

    uint8_t meta[META_BYTES];
    if (storage->loadMeta(meta, sizeof(meta)) == META_BYTES && memcmp(meta, META_MAGIC, 4) == 0 && meta[4] == META_VERSION &&
        crc32(meta, META_BYTES - 4) == get32(meta + META_BYTES - 4)) {
        const uint8_t *p = meta + 5;
        nextSeq = get32(p);
        p += 4;
        for (uint8_t t = 0; t < TIER_COUNT; t++, p += 4)
            st.enqueued[t] = get32(p);
        for (uint8_t t = 0; t < TIER_COUNT; t++, p += 4)
            st.published[t] = get32(p);
        for (uint8_t t = 0; t < TIER_COUNT; t++, p += 4)
            st.shed[t] = get32(p);
        st.p0Refused = get32(p);
        st.flashWriteFailed = get32(p + 4);
        st.restoreDiscarded = get32(p + 8);
    }

    size_t size = storage->size();
    size_t off = 0;
    std::vector<uint8_t> rec;
    bool sawSeq = false;
    uint32_t maxSeq = 0;
    while (off + RECORD_HEADER_BYTES <= size) {
        uint8_t hdr[RECORD_HEADER_BYTES];
        if (!storage->read(off, hdr, sizeof(hdr)) || hdr[0] != RECORD_MAGIC ||
            (hdr[1] != RECORD_DATA && hdr[1] != RECORD_TOMBSTONE))
            break;
        size_t bodyLen = get16(hdr + 2);
        size_t total = RECORD_HEADER_BYTES + bodyLen + RECORD_TRAILER_BYTES;
        if (bodyLen > cfg.maxRecordBytes || off + total > size)
            break;
        rec.resize(total);
        if (!storage->read(off, &rec[0], total) || checkFrame(&rec[0], total) < 0)
            break;

        if (hdr[1] == RECORD_DATA) {
            DataRecord d;
            if (!decodeData(&rec[0], total, d))
                break;
            if (findIndex(d.seq) < 0) {
                Entry e;
                e.seq = d.seq;
                e.packetId = d.packetId;
                e.offset = (uint32_t)off;
                e.len = (uint16_t)total;
                e.tier = d.tier;
                e.flags = d.flags;
                entries.push_back(e);
                liveFlashBytes += (uint32_t)total;
            }
            if (!sawSeq || d.seq > maxSeq)
                maxSeq = d.seq;
            sawSeq = true;
        } else {
            size_t count = get16(&rec[RECORD_HEADER_BYTES]);
            if (2 + 4 * count != bodyLen)
                break;
            for (size_t k = 0; k < count; k++) {
                int i = findIndex(get32(&rec[RECORD_HEADER_BYTES + 2 + 4 * k]));
                if (i >= 0) {
                    liveFlashBytes -= entries[i].len;
                    std::swap(entries[i], entries.back());
                    entries.pop_back();
                }
            }
        }
        off += total;
    }

    if (sawSeq && maxSeq + 1 > nextSeq)
        nextSeq = maxSeq + 1;

    if (off < size) {
        // REQ-ERR-03: keep the valid prefix, count and cut the rest.
        st.restoreDiscarded += (uint32_t)(size - off);
        needCompact = true;
    }
    if (needCompact) {
        if (!compact())
            flashOk = false; // never append after a corrupt tail
    } else {
        compactIfNeeded(0);
    }
}

Health QueueCore::health(uint32_t uptimeS) const
{
    Health h;
    memset(&h, 0, sizeof(h));
    h.uptimeS = uptimeS;
    h.capacity = (uint32_t)capacity();
    for (uint8_t t = 0; t < TIER_COUNT; t++) {
        size_t d = depth((Tier)t);
        h.depth[t] = (uint16_t)std::min<size_t>(d, 0xFFFF);
        h.enqueued[t] = st.enqueued[t];
        h.published[t] = st.published[t];
        h.shed[t] = st.shed[t];
    }
    h.p0Refused = st.p0Refused;
    h.flashWriteFailed = st.flashWriteFailed;
    h.restoreDiscarded = st.restoreDiscarded;
    h.flashBytes = (uint32_t)flashBytes();
    h.flashBudget = cfg.flashBudgetBytes;
    return h;
}

void encodeHealth(const Health &h, std::vector<uint8_t> &out)
{
    out.assign(HEALTH_MAGIC, HEALTH_MAGIC + 4);
    out.push_back(HEALTH_VERSION);
    out.push_back(0); // reserved
    put32(out, h.uptimeS);
    put32(out, h.capacity);
    for (uint8_t t = 0; t < TIER_COUNT; t++)
        put16(out, h.depth[t]);
    for (uint8_t t = 0; t < TIER_COUNT; t++)
        put32(out, h.enqueued[t]);
    for (uint8_t t = 0; t < TIER_COUNT; t++)
        put32(out, h.published[t]);
    for (uint8_t t = 0; t < TIER_COUNT; t++)
        put32(out, h.shed[t]);
    put32(out, h.p0Refused);
    put32(out, h.flashWriteFailed);
    put32(out, h.restoreDiscarded);
    put32(out, h.flashBytes);
    put32(out, h.flashBudget);
}

bool decodeHealth(const uint8_t *d, size_t len, Health &h)
{
    if (d == NULL || len != HEALTH_BYTES || memcmp(d, HEALTH_MAGIC, 4) != 0 || d[4] != HEALTH_VERSION)
        return false;
    const uint8_t *p = d + 6;
    h.uptimeS = get32(p);
    h.capacity = get32(p + 4);
    p += 8;
    for (uint8_t t = 0; t < TIER_COUNT; t++, p += 2)
        h.depth[t] = get16(p);
    for (uint8_t t = 0; t < TIER_COUNT; t++, p += 4)
        h.enqueued[t] = get32(p);
    for (uint8_t t = 0; t < TIER_COUNT; t++, p += 4)
        h.published[t] = get32(p);
    for (uint8_t t = 0; t < TIER_COUNT; t++, p += 4)
        h.shed[t] = get32(p);
    h.p0Refused = get32(p);
    h.flashWriteFailed = get32(p + 4);
    h.restoreDiscarded = get32(p + 8);
    h.flashBytes = get32(p + 12);
    h.flashBudget = get32(p + 16);
    return true;
}

} // namespace oq
} // namespace treklink
