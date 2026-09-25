/*
 * TrekLink onboard-queue configuration (specs/onboard-queue, REQ-UBI-05, D-015).
 *
 * Every business parameter of the durable MQTT outbound queue lives here as a
 * build_flags-overridable constant, following MESSAGE_HISTORY_LIMIT in MessageStore.h.
 * Override per variant in variants/.../platformio.ini, for example:
 *   -D TREKLINK_OQ_MAX_ENTRIES=1000
 *
 * The defaults are the v3 (T-Beam, 1 MiB LittleFS) values; v2 and v4 raise them.
 * Register any change in the Configuration Matrix.
 */
#pragma once

// Total entries across RAM and flash while flash is healthy (REQ-STA-01).
#ifndef TREKLINK_OQ_MAX_ENTRIES
#define TREKLINK_OQ_MAX_ENTRIES 200
#endif

// Entries whose record is held in RAM; also the total bound while flash is failing (REQ-STA-05).
#ifndef TREKLINK_OQ_RAM_ENTRIES
#define TREKLINK_OQ_RAM_ENTRIES 32
#endif

// RAM entries moved to flash in one write when the RAM bound is exceeded (REQ-EVT-05). 0 means RAM_ENTRIES / 4.
#ifndef TREKLINK_OQ_SPILL_BATCH
#define TREKLINK_OQ_SPILL_BATCH 0
#endif

// Upper bound on the flash log, clamped at init against free LittleFS space (REQ-ERR-06).
#ifndef TREKLINK_OQ_FLASH_BUDGET_BYTES
#define TREKLINK_OQ_FLASH_BUDGET_BYTES (256u * 1024u)
#endif

// LittleFS space always left free for NodeDB and preferences when clamping the budget.
#ifndef TREKLINK_OQ_FS_RESERVE_BYTES
#define TREKLINK_OQ_FS_RESERVE_BYTES (64u * 1024u)
#endif

// Minimum time between two drained publishes (REQ-STA-03).
#ifndef TREKLINK_OQ_DRAIN_INTERVAL_MS
#define TREKLINK_OQ_DRAIN_INTERVAL_MS 200
#endif

// SOS-episode exemption: the first K and latest M episode entries are never shed (REQ-STA-04).
#ifndef TREKLINK_OQ_EPISODE_KEEP_FIRST
#define TREKLINK_OQ_EPISODE_KEEP_FIRST 3
#endif
#ifndef TREKLINK_OQ_EPISODE_KEEP_LATEST
#define TREKLINK_OQ_EPISODE_KEEP_LATEST 5
#endif

// Deletion markers batched per flash write (design.md section 1.2).
#ifndef TREKLINK_OQ_TOMBSTONE_BATCH
#define TREKLINK_OQ_TOMBSTONE_BATCH 16
#endif

// Compaction runs once the log is at least this large and this share of it is dead.
#ifndef TREKLINK_OQ_COMPACT_MIN_BYTES
#define TREKLINK_OQ_COMPACT_MIN_BYTES (16u * 1024u)
#endif
#ifndef TREKLINK_OQ_COMPACT_DEAD_PERCENT
#define TREKLINK_OQ_COMPACT_DEAD_PERCENT 50
#endif

// Queue-health report interval while the link is up (REQ-EVT-12).
#ifndef TREKLINK_OQ_HEALTH_INTERVAL_S
#define TREKLINK_OQ_HEALTH_INTERVAL_S 300
#endif

// A record body larger than this is treated as corruption at restore (design.md section 1.1).
#ifndef TREKLINK_OQ_MAX_RECORD_BYTES
#define TREKLINK_OQ_MAX_RECORD_BYTES 1024
#endif
