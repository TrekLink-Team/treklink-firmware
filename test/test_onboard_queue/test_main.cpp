// Unit tests for the TrekLink onboard-queue policy core (specs/onboard-queue, tasks 1.5, 2.3, 3.8, 3.9, 4.8, 4.9).
//
// The core is platform-free, so this suite runs both under PlatformIO (`pio test -e native -f test_onboard_queue`)
// and on a bare host compiler; see the note at the bottom of this file.

#include "mqtt/TrekLinkQueueCore.h"

#include <algorithm>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

using namespace treklink::oq;

static const char *SOS_PREFIX = "SOS - "; // mirrors TREKLINK_SOS_TEXT_PREFIX; the classifier receives it as a parameter

// ---------------------------------------------------------------------------
// In-memory LogStorage with fault injection
// ---------------------------------------------------------------------------

class MemStorage : public LogStorage
{
  public:
    std::vector<uint8_t> log, meta, next;
    int failAppends;  // number of upcoming appends that fail
    size_t tornBytes; // bytes a failing append still writes
    bool failRewrite;
    size_t appendCalls;

    MemStorage() : failAppends(0), tornBytes(0), failRewrite(false), appendCalls(0) {}

    size_t size() { return log.size(); }
    bool append(const uint8_t *d, size_t n)
    {
        appendCalls++;
        if (failAppends > 0) {
            failAppends--;
            log.insert(log.end(), d, d + std::min(n, tornBytes));
            return false;
        }
        log.insert(log.end(), d, d + n);
        return true;
    }
    bool read(size_t off, uint8_t *buf, size_t n)
    {
        if (off + n > log.size())
            return false;
        memcpy(buf, &log[off], n);
        return true;
    }
    bool beginRewrite()
    {
        next.clear();
        return !failRewrite;
    }
    bool rewriteWrite(const uint8_t *d, size_t n)
    {
        next.insert(next.end(), d, d + n);
        return true;
    }
    bool endRewrite()
    {
        log.swap(next);
        next.clear();
        return true;
    }
    bool saveMeta(const uint8_t *d, size_t n)
    {
        meta.assign(d, d + n);
        return true;
    }
    size_t loadMeta(uint8_t *buf, size_t cap)
    {
        if (meta.empty() || meta.size() > cap)
            return 0;
        memcpy(buf, &meta[0], meta.size());
        return meta.size();
    }
};

static Config testConfig(uint16_t maxEntries = 16, uint16_t ramEntries = 16)
{
    Config c = defaultConfig();
    c.maxEntries = maxEntries;
    c.ramEntries = ramEntries;
    c.spillBatch = 0;
    c.flashBudgetBytes = 1u << 20;
    c.episodeKeepFirst = 3;
    c.episodeKeepLatest = 5;
    c.tombstoneBatch = 4;
    c.compactMinBytes = 1u << 30; // off unless a test lowers it
    c.compactDeadPercent = 50;
    c.maxRecordBytes = 1024;
    return c;
}

static const std::string TOPIC = "treklink/2/e/LongFast/!a1b2c3d4";

static EnqueueResult put(QueueCore &q, Tier t, uint32_t pid = 0, bool episode = false, size_t envLen = 40)
{
    std::vector<uint8_t> env(envLen);
    for (size_t i = 0; i < envLen; i++)
        env[i] = (uint8_t)(pid + i);
    return q.enqueue(t, episode, pid, TOPIC, envLen ? &env[0] : NULL, envLen);
}

// Drains the whole queue, returning packet ids in publish order.
static std::vector<uint32_t> drainAll(QueueCore &q)
{
    std::vector<uint32_t> out;
    Pending p;
    while (q.peek(p)) {
        q.markInFlight(p.seq);
        q.commit(p.seq);
        out.push_back(p.packetId);
    }
    return out;
}

static uint32_t sum(const uint32_t *a)
{
    return a[0] + a[1] + a[2] + a[3];
}

static void assertIdentity(QueueCore &q)
{
    const Stats &s = q.stats();
    TEST_ASSERT_EQUAL_UINT32(sum(s.enqueued), sum(s.published) + sum(s.shed) + s.p0Refused + (uint32_t)q.depth());
}

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// Task 1.5: record codec
// ---------------------------------------------------------------------------

void test_record_round_trip(void)
{
    std::vector<uint8_t> env(531); // the largest envelope MQTT.cpp can encode
    for (size_t i = 0; i < env.size(); i++)
        env[i] = (uint8_t)(i * 7);
    std::vector<uint8_t> rec;
    encodeData(rec, 42, 0xDEADBEEF, TIER_P1, FLAG_EPISODE, TOPIC, &env[0], env.size());

    DataRecord d;
    TEST_ASSERT_TRUE(decodeData(&rec[0], rec.size(), d));
    TEST_ASSERT_EQUAL_UINT32(42, d.seq);
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEF, d.packetId);
    TEST_ASSERT_EQUAL(TIER_P1, d.tier);
    TEST_ASSERT_EQUAL_UINT8(FLAG_EPISODE, d.flags);
    TEST_ASSERT_EQUAL_STRING(TOPIC.c_str(), d.topic.c_str());
    TEST_ASSERT_EQUAL(env.size(), d.env.size());
    TEST_ASSERT_EQUAL_MEMORY(&env[0], &d.env[0], env.size());
}

void test_record_rejects_corruption(void)
{
    std::vector<uint8_t> env(20, 0x55);
    std::vector<uint8_t> rec;
    encodeData(rec, 1, 2, TIER_P3, 0, TOPIC, &env[0], env.size());
    DataRecord d;

    std::vector<uint8_t> flipped = rec;
    flipped[10] ^= 0x01;
    TEST_ASSERT_FALSE(decodeData(&flipped[0], flipped.size(), d));

    TEST_ASSERT_FALSE(decodeData(&rec[0], rec.size() - 1, d)); // truncated tail

    std::vector<uint8_t> tomb;
    std::vector<uint32_t> seqs(1, 7);
    encodeTombstones(tomb, seqs);
    TEST_ASSERT_FALSE(decodeData(&tomb[0], tomb.size(), d)); // wrong type
}

void test_oversize_record_is_refused(void)
{
    QueueCore q(testConfig(), NULL);
    TEST_ASSERT_EQUAL(ENQ_REFUSED, put(q, TIER_P3, 1, false, 2000));
    TEST_ASSERT_EQUAL(ENQ_REFUSED_P0, put(q, TIER_P0, 2, false, 2000));
    TEST_ASSERT_EQUAL(0, q.depth());
    TEST_ASSERT_EQUAL_UINT32(1, q.stats().shed[TIER_P3]);
    TEST_ASSERT_EQUAL_UINT32(1, q.stats().p0Refused);
    assertIdentity(q);
}

// ---------------------------------------------------------------------------
// Task 2.3: classification
// ---------------------------------------------------------------------------

static Tier cls(bool decoded, uint32_t port, uint8_t prio, bool fromUs, bool episode, const char *text)
{
    ClassifyInput in;
    in.decoded = decoded;
    in.portnum = port;
    in.priority = prio;
    in.fromUs = fromUs;
    in.episodeActive = episode;
    in.payload = (const uint8_t *)text;
    in.payloadLen = text ? strlen(text) : 0;
    return classify(in, SOS_PREFIX);
}

void test_classify_matrix(void)
{
    const uint8_t HIGH = 70, DEFAULT_PRIO = 64, BACKGROUND = 10;
    const uint32_t TELEMETRY = 67, PRIVATE_APP = 256;

    TEST_ASSERT_EQUAL(TIER_P3, cls(false, PORTNUM_TEXT_MESSAGE_APP, PRIORITY_MAX, true, true, "SOS - [No GPS]"));
    // own SOS, by prefix and by MAX
    TEST_ASSERT_EQUAL(TIER_P0, cls(true, PORTNUM_TEXT_MESSAGE_APP, PRIORITY_MAX, true, false, "SOS - [No GPS]"));
    TEST_ASSERT_EQUAL(TIER_P0, cls(true, PORTNUM_TEXT_MESSAGE_APP, PRIORITY_MAX, true, false, "reformatted"));
    TEST_ASSERT_EQUAL(TIER_P0, cls(true, PORTNUM_TEXT_MESSAGE_APP, PRIORITY_MAX, true, false,
                                   "SOS - FALL DETECTED - [11.123456], [107.654321]"));
    // peer SOS arrives as HIGH: prefix only (AC-13)
    TEST_ASSERT_EQUAL(TIER_P0, cls(true, PORTNUM_TEXT_MESSAGE_APP, HIGH, false, false, "SOS - [1.0], [2.0]"));
    TEST_ASSERT_EQUAL(TIER_P3, cls(true, PORTNUM_TEXT_MESSAGE_APP, PRIORITY_MAX, false, false, "hello"));
    TEST_ASSERT_EQUAL(TIER_P3, cls(true, PORTNUM_TEXT_MESSAGE_APP, HIGH, true, true, "hello"));
    TEST_ASSERT_EQUAL(TIER_P3, cls(true, PORTNUM_TEXT_MESSAGE_APP, HIGH, false, false, "SOS"));
    // positions (AC-12)
    TEST_ASSERT_EQUAL(TIER_P1, cls(true, PORTNUM_POSITION_APP, BACKGROUND, true, true, NULL));
    TEST_ASSERT_EQUAL(TIER_P1, cls(true, PORTNUM_POSITION_APP, PRIORITY_MAX, true, false, NULL));
    TEST_ASSERT_EQUAL(TIER_P2, cls(true, PORTNUM_POSITION_APP, BACKGROUND, true, false, NULL));
    TEST_ASSERT_EQUAL(TIER_P2, cls(true, PORTNUM_POSITION_APP, DEFAULT_PRIO, false, true, NULL));
    TEST_ASSERT_EQUAL(TIER_P2, cls(true, PORTNUM_POSITION_APP, PRIORITY_MAX, false, false, NULL));
    // everything else
    TEST_ASSERT_EQUAL(TIER_P3, cls(true, TELEMETRY, BACKGROUND, true, true, NULL));
    TEST_ASSERT_EQUAL(TIER_P3, cls(true, PRIVATE_APP, PRIORITY_MAX, true, true, "SOS - x"));
}

// ---------------------------------------------------------------------------
// Tasks 3.8 and 3.9: ordering and shedding
// ---------------------------------------------------------------------------

void test_flush_order_randomised(void)
{
    srand(1234);
    for (int round = 0; round < 20; round++) {
        QueueCore q(testConfig(500, 500), NULL);
        std::vector<std::pair<int, uint32_t>> expected; // tier, pid in enqueue order
        for (uint32_t i = 0; i < 200; i++) {
            Tier t = (Tier)(rand() % 4);
            TEST_ASSERT_EQUAL(ENQ_ACCEPTED, put(q, t, i));
            expected.push_back(std::make_pair((int)t, i));
        }
        std::stable_sort(expected.begin(), expected.end(),
                         [](const std::pair<int, uint32_t> &a, const std::pair<int, uint32_t> &b) { return a.first < b.first; });
        std::vector<uint32_t> got = drainAll(q);
        TEST_ASSERT_EQUAL(expected.size(), got.size());
        for (size_t i = 0; i < got.size(); i++)
            TEST_ASSERT_EQUAL_UINT32(expected[i].second, got[i]);
        assertIdentity(q);
    }
}

// AC-01 and AC-05: 16 telemetry entries then one SOS keeps the SOS and sheds telemetry.
void test_sos_survives_full_telemetry_queue(void)
{
    QueueCore q(testConfig(16, 16), NULL);
    for (uint32_t i = 0; i < 16; i++)
        put(q, TIER_P3, i);
    TEST_ASSERT_EQUAL(ENQ_ACCEPTED_AFTER_SHED, put(q, TIER_P0, 100));
    TEST_ASSERT_EQUAL(16, q.depth());
    TEST_ASSERT_EQUAL_UINT32(1, q.stats().shed[TIER_P3]);

    std::vector<uint32_t> order = drainAll(q);
    TEST_ASSERT_EQUAL_UINT32(100, order[0]);
    // Newest-first shedding: telemetry 0..14 survive, 15 was shed.
    for (uint32_t i = 0; i < 15; i++)
        TEST_ASSERT_EQUAL_UINT32(i, order[1 + i]);
    assertIdentity(q);
}

void test_newest_in_lowest_tier_is_shed(void)
{
    QueueCore q(testConfig(4, 4), NULL);
    put(q, TIER_P2, 1);
    put(q, TIER_P3, 2);
    put(q, TIER_P3, 3);
    put(q, TIER_P2, 4);
    TEST_ASSERT_EQUAL(ENQ_ACCEPTED_AFTER_SHED, put(q, TIER_P1, 5)); // sheds P3 #3, the newest P3
    TEST_ASSERT_EQUAL(ENQ_ACCEPTED_AFTER_SHED, put(q, TIER_P1, 6)); // sheds P3 #2
    // Lowest occupied tier is now P2, and the incoming #7 is its newest candidate: refused.
    TEST_ASSERT_EQUAL(ENQ_REFUSED, put(q, TIER_P2, 7));
    std::vector<uint32_t> order = drainAll(q);
    TEST_ASSERT_EQUAL(4, order.size());
    TEST_ASSERT_EQUAL_UINT32(5, order[0]);
    TEST_ASSERT_EQUAL_UINT32(6, order[1]);
    TEST_ASSERT_EQUAL_UINT32(1, order[2]);
    TEST_ASSERT_EQUAL_UINT32(4, order[3]);
}

void test_incoming_refused_when_it_is_the_victim(void)
{
    QueueCore q(testConfig(3, 3), NULL);
    put(q, TIER_P2, 1);
    put(q, TIER_P2, 2);
    put(q, TIER_P1, 3);
    TEST_ASSERT_EQUAL(ENQ_REFUSED, put(q, TIER_P3, 4));
    TEST_ASSERT_EQUAL(ENQ_REFUSED, put(q, TIER_P2, 5));
    TEST_ASSERT_EQUAL_UINT32(1, q.stats().shed[TIER_P3]);
    TEST_ASSERT_EQUAL_UINT32(1, q.stats().shed[TIER_P2]);
    TEST_ASSERT_EQUAL(3, q.depth());
    assertIdentity(q);
}

// AC-06
void test_all_p0_queue_refuses_p0(void)
{
    QueueCore q(testConfig(4, 4), NULL);
    for (uint32_t i = 0; i < 4; i++)
        put(q, TIER_P0, i);
    TEST_ASSERT_EQUAL(ENQ_REFUSED_P0, put(q, TIER_P0, 9));
    TEST_ASSERT_EQUAL(ENQ_REFUSED, put(q, TIER_P3, 10));
    TEST_ASSERT_EQUAL_UINT32(1, q.stats().p0Refused);
    TEST_ASSERT_EQUAL_UINT32(0, q.stats().shed[TIER_P0]);
    std::vector<uint32_t> order = drainAll(q);
    TEST_ASSERT_EQUAL(4, order.size());
    for (uint32_t i = 0; i < 4; i++)
        TEST_ASSERT_EQUAL_UINT32(i, order[i]);
    assertIdentity(q);
}

void test_episode_exemption_is_bounded(void)
{
    Config c = testConfig(6, 6);
    c.episodeKeepFirst = 1;
    c.episodeKeepLatest = 2;
    QueueCore q(c, NULL);
    q.setEpisodeActive(true);
    for (uint32_t i = 0; i < 6; i++)
        put(q, TIER_P1, i, true);
    // Kept: first (#0) and latest two (#5 and incoming #6). Middle #1..#4 are sheddable; newest of them is #4.
    TEST_ASSERT_EQUAL(ENQ_ACCEPTED_AFTER_SHED, put(q, TIER_P1, 6, true));
    TEST_ASSERT_EQUAL(ENQ_ACCEPTED_AFTER_SHED, put(q, TIER_P1, 7, true)); // then #5 is middle, newest middle is #5
    std::vector<uint32_t> order = drainAll(q);
    uint32_t want[] = {0, 1, 2, 3, 6, 7};
    TEST_ASSERT_EQUAL(6, order.size());
    for (int i = 0; i < 6; i++)
        TEST_ASSERT_EQUAL_UINT32(want[i], order[i]);
}

void test_episode_exemption_only_while_active(void)
{
    QueueCore q(testConfig(3, 3), NULL);
    for (uint32_t i = 0; i < 3; i++)
        put(q, TIER_P1, i, true);
    // No active episode: the incoming P1 is the newest P1 and is refused.
    TEST_ASSERT_EQUAL(ENQ_REFUSED, put(q, TIER_P1, 3, true));
}

void test_episode_cannot_starve_a_new_sos(void)
{
    Config c = testConfig(8, 8);
    c.episodeKeepFirst = 3;
    c.episodeKeepLatest = 5;
    QueueCore q(c, NULL);
    q.setEpisodeActive(true);
    for (uint32_t i = 0; i < 40; i++)
        put(q, TIER_P1, i, true); // an indefinitely beaconing node
    TEST_ASSERT_EQUAL(8, q.depth());
    // K + M = 8 would exempt the whole queue; the core clamps K + M below the RAM bound, so one entry stays sheddable.
    TEST_ASSERT_EQUAL(ENQ_ACCEPTED_AFTER_SHED, put(q, TIER_P0, 1000)); // a peer SOS still gets in
    TEST_ASSERT_EQUAL(1, q.depth(TIER_P0));
}

void test_in_flight_entry_is_never_shed(void)
{
    QueueCore q(testConfig(2, 2), NULL);
    put(q, TIER_P3, 1); // seq 0
    put(q, TIER_P3, 2); // seq 1, the newest P3 and normally the victim
    q.markInFlight(1);
    TEST_ASSERT_EQUAL(ENQ_ACCEPTED_AFTER_SHED, put(q, TIER_P2, 3));
    q.commit(1);
    std::vector<uint32_t> order = drainAll(q);
    TEST_ASSERT_EQUAL(1, order.size());
    TEST_ASSERT_EQUAL_UINT32(3, order[0]);
    TEST_ASSERT_EQUAL_UINT32(1, q.stats().shed[TIER_P3]);
    TEST_ASSERT_EQUAL_UINT32(1, q.stats().published[TIER_P3]);
    assertIdentity(q);
}

void test_release_keeps_entry_queued(void)
{
    QueueCore q(testConfig(), NULL);
    put(q, TIER_P0, 7);
    Pending p;
    TEST_ASSERT_TRUE(q.peek(p));
    q.markInFlight(p.seq);
    q.release(p.seq); // link dropped before the next poll
    TEST_ASSERT_EQUAL(1, q.depth());
    TEST_ASSERT_TRUE(q.peek(p));
    TEST_ASSERT_EQUAL_UINT32(7, p.packetId);
}

void test_counter_identity_random_ops(void)
{
    MemStorage s;
    Config c = testConfig(30, 8);
    c.episodeKeepFirst = 2;
    c.episodeKeepLatest = 2;
    QueueCore q(c, &s);
    srand(99);
    for (int i = 0; i < 2000; i++) {
        int op = rand() % 10;
        if (op < 7) {
            put(q, (Tier)(rand() % 4), (uint32_t)i, rand() % 3 == 0);
        } else if (op < 9) {
            Pending p;
            if (q.peek(p)) {
                q.markInFlight(p.seq);
                if (rand() % 4)
                    q.commit(p.seq);
                else
                    q.release(p.seq);
            }
        } else {
            q.setEpisodeActive(rand() % 2);
        }
        assertIdentity(q);
        TEST_ASSERT_TRUE(q.depth() <= q.capacity());
    }
}

// ---------------------------------------------------------------------------
// Task 4.8: flash tier and durability
// ---------------------------------------------------------------------------

void test_p0_p1_write_through_p2_p3_in_ram(void)
{
    MemStorage s;
    QueueCore q(testConfig(16, 8), &s);
    q.restore();
    put(q, TIER_P0, 1);
    TEST_ASSERT_TRUE(s.log.size() > 0);
    put(q, TIER_P1, 2);
    size_t afterSos = s.log.size();
    put(q, TIER_P2, 3);
    put(q, TIER_P3, 4);
    TEST_ASSERT_EQUAL(afterSos, s.log.size());
    TEST_ASSERT_EQUAL(2, q.ramResident());
}

void test_spill_moves_lowest_oldest_in_one_write(void)
{
    MemStorage s;
    Config c = testConfig(16, 4);
    c.spillBatch = 2;
    QueueCore q(c, &s);
    q.restore();
    put(q, TIER_P2, 10);
    put(q, TIER_P3, 11);
    put(q, TIER_P3, 12);
    put(q, TIER_P2, 13);
    size_t calls = s.appendCalls;
    put(q, TIER_P3, 14); // RAM holds 5 > 4: spill two
    TEST_ASSERT_EQUAL(calls + 1, s.appendCalls);
    TEST_ASSERT_EQUAL(3, q.ramResident());

    // The two spilled records are the oldest P3 entries, #11 then #12.
    DataRecord a, b;
    size_t lenA = 4 + (size_t)(s.log[2] | (s.log[3] << 8)) + 4;
    TEST_ASSERT_TRUE(decodeData(&s.log[0], lenA, a));
    TEST_ASSERT_TRUE(decodeData(&s.log[lenA], s.log.size() - lenA, b));
    TEST_ASSERT_EQUAL_UINT32(11, a.packetId);
    TEST_ASSERT_EQUAL_UINT32(12, b.packetId);
}

// AC-03: orderly persistence then restore loses nothing and keeps order and counters.
void test_persist_and_restore(void)
{
    MemStorage s;
    uint32_t enq3;
    {
        QueueCore q(testConfig(32, 4), &s);
        q.restore();
        for (uint32_t i = 0; i < 10; i++)
            put(q, (Tier)(i % 4), i);
        Pending p;
        q.peek(p);
        q.markInFlight(p.seq);
        q.commit(p.seq); // publishes #0 (P0)
        enq3 = q.stats().enqueued[TIER_P3];
        q.persistAll();
    }
    QueueCore r(testConfig(32, 4), &s);
    r.restore();
    TEST_ASSERT_EQUAL(9, r.depth());
    TEST_ASSERT_EQUAL_UINT32(enq3, r.stats().enqueued[TIER_P3]);
    TEST_ASSERT_EQUAL_UINT32(1, r.stats().published[TIER_P0]);
    assertIdentity(r);

    put(r, TIER_P0, 99);
    std::vector<uint32_t> order = drainAll(r);
    uint32_t want[] = {4, 8, 99, 1, 5, 9, 2, 6, 3, 7};
    TEST_ASSERT_EQUAL(10, order.size());
    for (int i = 0; i < 10; i++)
        TEST_ASSERT_EQUAL_UINT32(want[i], order[i]);
}

// AC-04: without persistAll(), SOS entries survive and unspilled P2/P3 are the accepted loss.
void test_unclean_power_loss(void)
{
    MemStorage s;
    {
        QueueCore q(testConfig(32, 8), &s);
        q.restore();
        put(q, TIER_P3, 1);
        put(q, TIER_P0, 2);
        put(q, TIER_P2, 3);
        put(q, TIER_P1, 4);
    }
    QueueCore r(testConfig(32, 8), &s);
    r.restore();
    std::vector<uint32_t> order = drainAll(r);
    TEST_ASSERT_EQUAL(2, order.size());
    TEST_ASSERT_EQUAL_UINT32(2, order[0]);
    TEST_ASSERT_EQUAL_UINT32(4, order[1]);
}

void test_tombstones_hide_published_entries_after_restore(void)
{
    MemStorage s;
    {
        QueueCore q(testConfig(32, 8), &s);
        q.restore();
        for (uint32_t i = 0; i < 6; i++)
            put(q, TIER_P0, i);
        for (int k = 0; k < 3; k++) {
            Pending p;
            q.peek(p);
            q.markInFlight(p.seq);
            q.commit(p.seq);
        }
        q.persistAll();
    }
    QueueCore r(testConfig(32, 8), &s);
    r.restore();
    std::vector<uint32_t> order = drainAll(r);
    TEST_ASSERT_EQUAL(3, order.size());
    TEST_ASSERT_EQUAL_UINT32(3, order[0]);
}

// REQ-ERR-04: tombstones not yet flushed mean a re-publish, never a loss.
void test_unflushed_tombstone_republishes(void)
{
    MemStorage s;
    {
        QueueCore q(testConfig(32, 8), &s); // tombstoneBatch = 4
        q.restore();
        put(q, TIER_P0, 1);
        put(q, TIER_P0, 2);
        Pending p;
        q.peek(p);
        q.markInFlight(p.seq);
        q.commit(p.seq); // one pending tombstone, below the batch
    }
    QueueCore r(testConfig(32, 8), &s);
    r.restore();
    std::vector<uint32_t> order = drainAll(r);
    TEST_ASSERT_EQUAL(2, order.size());
    TEST_ASSERT_EQUAL_UINT32(1, order[0]);
}

// AC-07: a truncated tail keeps the valid prefix, is counted, and is cut so later appends restore.
void test_truncated_log_restores_valid_prefix(void)
{
    MemStorage s;
    {
        QueueCore q(testConfig(32, 8), &s);
        q.restore();
        for (uint32_t i = 0; i < 3; i++)
            put(q, TIER_P0, i);
    }
    s.log.resize(s.log.size() - 5);
    {
        QueueCore r(testConfig(32, 8), &s);
        r.restore();
        TEST_ASSERT_EQUAL(2, r.depth());
        TEST_ASSERT_TRUE(r.stats().restoreDiscarded > 0);
        TEST_ASSERT_TRUE(r.flashHealthy());
        put(r, TIER_P0, 50);
    }
    QueueCore again(testConfig(32, 8), &s);
    again.restore();
    std::vector<uint32_t> order = drainAll(again);
    TEST_ASSERT_EQUAL(3, order.size());
    TEST_ASSERT_EQUAL_UINT32(50, order[2]);
}

void test_mid_log_crc_fault_discards_the_rest(void)
{
    MemStorage s;
    {
        QueueCore q(testConfig(32, 8), &s);
        q.restore();
        for (uint32_t i = 0; i < 3; i++)
            put(q, TIER_P0, i);
    }
    size_t first = 4 + (size_t)(s.log[2] | (s.log[3] << 8)) + 4;
    s.log[first + 8] ^= 0xFF; // corrupt the second record's body
    size_t before = s.log.size();
    QueueCore r(testConfig(32, 8), &s);
    r.restore();
    TEST_ASSERT_EQUAL(1, r.depth());
    TEST_ASSERT_EQUAL_UINT32(before - first, r.stats().restoreDiscarded);
    TEST_ASSERT_EQUAL(first, s.log.size());
}

void test_restore_keeps_seq_monotonic(void)
{
    MemStorage s;
    {
        QueueCore q(testConfig(32, 8), &s);
        q.restore();
        put(q, TIER_P1, 1);
        put(q, TIER_P1, 2);
    }
    QueueCore r(testConfig(32, 8), &s);
    r.restore();
    put(r, TIER_P1, 3);
    std::vector<uint32_t> order = drainAll(r);
    TEST_ASSERT_EQUAL_UINT32(1, order[0]);
    TEST_ASSERT_EQUAL_UINT32(2, order[1]);
    TEST_ASSERT_EQUAL_UINT32(3, order[2]);
}

void test_compaction_reclaims_dead_records(void)
{
    MemStorage s;
    Config c = testConfig(64, 8);
    c.compactMinBytes = 512;
    QueueCore q(c, &s);
    q.restore();
    for (uint32_t i = 0; i < 20; i++)
        put(q, TIER_P0, i);
    size_t full = s.log.size();
    for (int k = 0; k < 16; k++) {
        Pending p;
        q.peek(p);
        q.markInFlight(p.seq);
        q.commit(p.seq);
    }
    TEST_ASSERT_TRUE(s.log.size() < full);
    TEST_ASSERT_EQUAL(4, q.depth());
    q.persistAll(); // flush the last deletion markers, or those entries are re-published (REQ-ERR-04)

    QueueCore r(c, &s);
    r.restore();
    std::vector<uint32_t> order = drainAll(r);
    TEST_ASSERT_EQUAL(4, order.size());
    TEST_ASSERT_EQUAL_UINT32(16, order[0]);
}

// REQ-ERR-02, REQ-STA-05: a failing flash keeps entries in RAM and drops the bound to the RAM bound.
void test_flash_failure_falls_back_to_ram(void)
{
    MemStorage s;
    QueueCore q(testConfig(32, 4), &s);
    q.restore();
    s.failAppends = 1000;
    TEST_ASSERT_EQUAL(ENQ_ACCEPTED, put(q, TIER_P0, 1));
    TEST_ASSERT_EQUAL_UINT32(1, q.stats().flashWriteFailed);
    TEST_ASSERT_FALSE(q.flashHealthy());
    TEST_ASSERT_EQUAL(4, q.capacity());
    for (uint32_t i = 2; i < 10; i++)
        put(q, TIER_P3, i);
    TEST_ASSERT_TRUE(q.depth() <= 4);
    TEST_ASSERT_EQUAL(1, q.depth(TIER_P0));
    assertIdentity(q);

    s.failAppends = 0; // flash recovers on the next write
    put(q, TIER_P1, 20);
    TEST_ASSERT_TRUE(q.flashHealthy());
    TEST_ASSERT_EQUAL(32, q.capacity());
    // The P0 held in RAM during the fault is moved to flash as soon as flash works again.
    TEST_ASSERT_EQUAL(q.depth(TIER_P3), q.ramResident());
}

void test_torn_append_is_cut_before_the_next_write(void)
{
    MemStorage s;
    {
        QueueCore q(testConfig(32, 8), &s);
        q.restore();
        put(q, TIER_P0, 1);
        s.failAppends = 1;
        s.tornBytes = 7;
        put(q, TIER_P0, 2); // torn on flash, kept in RAM
        put(q, TIER_P0, 3); // must cut the torn bytes first
        q.persistAll();
    }
    QueueCore r(testConfig(32, 8), &s);
    r.restore();
    TEST_ASSERT_EQUAL_UINT32(0, r.stats().restoreDiscarded);
    std::vector<uint32_t> order = drainAll(r);
    TEST_ASSERT_EQUAL(3, order.size());
    TEST_ASSERT_EQUAL_UINT32(1, order[0]);
    TEST_ASSERT_EQUAL_UINT32(2, order[1]);
    TEST_ASSERT_EQUAL_UINT32(3, order[2]);
}

void test_budget_exhaustion(void)
{
    MemStorage s;
    Config c = testConfig(32, 4);
    c.flashBudgetBytes = 200; // about two records
    QueueCore q(c, &s);
    q.restore();
    put(q, TIER_P0, 1);
    put(q, TIER_P0, 2);
    put(q, TIER_P0, 3); // does not fit
    TEST_ASSERT_TRUE(s.log.size() <= 200);
    TEST_ASSERT_TRUE(q.stats().flashWriteFailed >= 1);
    TEST_ASSERT_EQUAL(3, q.depth());
}

void test_corrupt_meta_is_ignored(void)
{
    MemStorage s;
    {
        QueueCore q(testConfig(32, 8), &s);
        q.restore();
        put(q, TIER_P0, 1);
        q.persistAll();
    }
    s.meta[6] ^= 0x10;
    QueueCore r(testConfig(32, 8), &s);
    r.restore();
    TEST_ASSERT_EQUAL(1, r.depth());
    TEST_ASSERT_EQUAL_UINT32(0, r.stats().enqueued[TIER_P0]);
}

// ---------------------------------------------------------------------------
// Task 4.9: health payload
// ---------------------------------------------------------------------------

void test_health_snapshot_and_round_trip(void)
{
    MemStorage s;
    Config c = testConfig(8, 8);
    QueueCore q(c, &s);
    q.restore();
    put(q, TIER_P0, 1);
    put(q, TIER_P3, 2);
    Health h = q.health(42);
    TEST_ASSERT_EQUAL_UINT32(42, h.uptimeS);
    TEST_ASSERT_EQUAL_UINT32(8, h.capacity);
    TEST_ASSERT_EQUAL_UINT16(1, h.depth[TIER_P0]);
    TEST_ASSERT_EQUAL_UINT16(1, h.depth[TIER_P3]);
    TEST_ASSERT_EQUAL_UINT32(1, h.enqueued[TIER_P3]);
    TEST_ASSERT_EQUAL_UINT32(s.log.size(), h.flashBytes);
    TEST_ASSERT_EQUAL_UINT32(1u << 20, h.flashBudget);

    std::vector<uint8_t> wire;
    encodeHealth(h, wire);
    TEST_ASSERT_EQUAL(HEALTH_BYTES, wire.size());
    TEST_ASSERT_TRUE(wire.size() <= 233); // meshtastic_Data_payload_t
    Health back;
    TEST_ASSERT_TRUE(decodeHealth(&wire[0], wire.size(), back));
    TEST_ASSERT_EQUAL_MEMORY(&h, &back, sizeof(Health));
}

void test_health_decode_rejects_foreign_payloads(void)
{
    Health h;
    memset(&h, 0, sizeof(h));
    h.shed[3] = 0xFFFFFFFFu;
    std::vector<uint8_t> wire;
    encodeHealth(h, wire);
    Health out;
    TEST_ASSERT_FALSE(decodeHealth(&wire[0], wire.size() - 1, out)); // wrong length
    std::vector<uint8_t> bad = wire;
    bad[0] = 'X';
    TEST_ASSERT_FALSE(decodeHealth(&bad[0], bad.size(), out)); // wrong magic
    bad = wire;
    bad[4] = 2;
    TEST_ASSERT_FALSE(decodeHealth(&bad[0], bad.size(), out)); // unknown version
    const char *text = "{\"some\":\"other app\"}";
    TEST_ASSERT_FALSE(decodeHealth((const uint8_t *)text, strlen(text), out));
    TEST_ASSERT_FALSE(decodeHealth(NULL, 0, out));
}

// ---------------------------------------------------------------------------

static int runAll()
{
    UNITY_BEGIN();
    RUN_TEST(test_record_round_trip);
    RUN_TEST(test_record_rejects_corruption);
    RUN_TEST(test_oversize_record_is_refused);
    RUN_TEST(test_classify_matrix);
    RUN_TEST(test_flush_order_randomised);
    RUN_TEST(test_sos_survives_full_telemetry_queue);
    RUN_TEST(test_newest_in_lowest_tier_is_shed);
    RUN_TEST(test_incoming_refused_when_it_is_the_victim);
    RUN_TEST(test_all_p0_queue_refuses_p0);
    RUN_TEST(test_episode_exemption_is_bounded);
    RUN_TEST(test_episode_exemption_only_while_active);
    RUN_TEST(test_episode_cannot_starve_a_new_sos);
    RUN_TEST(test_in_flight_entry_is_never_shed);
    RUN_TEST(test_release_keeps_entry_queued);
    RUN_TEST(test_counter_identity_random_ops);
    RUN_TEST(test_p0_p1_write_through_p2_p3_in_ram);
    RUN_TEST(test_spill_moves_lowest_oldest_in_one_write);
    RUN_TEST(test_persist_and_restore);
    RUN_TEST(test_unclean_power_loss);
    RUN_TEST(test_tombstones_hide_published_entries_after_restore);
    RUN_TEST(test_unflushed_tombstone_republishes);
    RUN_TEST(test_truncated_log_restores_valid_prefix);
    RUN_TEST(test_mid_log_crc_fault_discards_the_rest);
    RUN_TEST(test_restore_keeps_seq_monotonic);
    RUN_TEST(test_compaction_reclaims_dead_records);
    RUN_TEST(test_flash_failure_falls_back_to_ram);
    RUN_TEST(test_torn_append_is_cut_before_the_next_write);
    RUN_TEST(test_budget_exhaustion);
    RUN_TEST(test_corrupt_meta_is_ignored);
    RUN_TEST(test_health_snapshot_and_round_trip);
    RUN_TEST(test_health_decode_rejects_foreign_payloads);
    return UNITY_END();
}

#ifdef TREKLINK_HOST_TEST
// Host build, no PlatformIO:
//   g++ -std=c++11 -DTREKLINK_HOST_TEST -Isrc -I<unity>/src test/test_onboard_queue/test_main.cpp
//       src/mqtt/TrekLinkQueueCore.cpp <unity>/src/unity.c
int main()
{
    return runAll();
}
#else
#include <Arduino.h> // declares setup() and loop() with the linkage portduino's main expects

void setup()
{
    exit(runAll());
}
void loop() {}
#endif
