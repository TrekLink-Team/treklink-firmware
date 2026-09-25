# On-device queue-health payload (consumer copy)

> **Source of truth**: `treklink-firmware/specs/onboard-queue/design.md` §2.4. This copy is for `gateway-sync` REQ-EVT-12 and REQ-EVT-13. If the two disagree, the firmware design wins and this copy is stale.

## Transport

| Property | Value |
|---|---|
| PortNum | `PRIVATE_APP` (256) |
| Radio | never transmitted over LoRa; published by the uplink node straight to MQTT |
| Protobuf topic | `<root>/2/e/<channelId>/<nodeId>`, a `ServiceEnvelope` whose `packet.decoded.payload` is the 90-byte binary record below |
| JSON topic | `<root>/2/json/<channelId>/<nodeId>`, with `"type": "treklink_queue_health"` and the object below as `payload` |
| `from` | the reporting node's `nodeNum`; `to` is broadcast |
| Queued while offline | no. A report is sent only while the uplink is up |
| Cadence | every 300 s while the link is up and something changed; once right after an outage; configuration (D-015) |
| Other `PRIVATE_APP` payloads | serialised exactly as stock Meshtastic on `/2/json/`: empty `type`, no `payload`. Ignore them |

A health report is **not** a field event. It carries a fresh `MeshPacket.id` like any packet, but it must create no `GatewayEvent` and never reach episode correlation (REQ-EVT-12).

> **Revised 2026-09-25 (firmware C-007).** The first copy said the `/2/e/` payload was JSON text of up to 400 bytes. `Data.payload` holds at most 233 bytes (`mesh.pb.h:761`), so `/2/e/` now carries a fixed binary record. **The JSON topic is unchanged**: same `type`, same object, same fields. A JSON-only consumer needs no change.

## Payload on `/2/json/`

The object below, as `payload`. Every field is always present.

```json
{
  "schema": "treklink.queue_health",
  "v": 1,
  "uptime_s": 5321,
  "capacity": 200,
  "depth": [1, 4, 0, 12],
  "enqueued": [1, 9, 40, 310],
  "published": [0, 5, 40, 250],
  "shed": [0, 0, 0, 48],
  "p0_refused": 0,
  "flash_write_failed": 0,
  "restore_discarded": 0,
  "flash_bytes": 4096,
  "flash_budget": 262144
}
```

| Field | Type | Meaning |
|---|---|---|
| `schema` | string | constant `treklink.queue_health`; reject anything else |
| `v` | integer | schema version, `1` |
| `uptime_s` | integer | seconds since the node booted; resets on reboot |
| `capacity` | integer | effective total queue bound right now; drops to the RAM bound while flash is failing |
| `depth` | 4 integers | entries queued per tier, index 0 = P0 (SOS), 1 = P1, 2 = P2, 3 = P3 |
| `enqueued` | 4 integers | entries offered to the queue per tier since first boot, accepted or refused |
| `published` | 4 integers | entries delivered from the queue per tier |
| `shed` | 4 integers | entries discarded by the shedding policy per tier; `shed[0]` is always 0 |
| `p0_refused` | integer | SOS entries refused because the queue was full of entries that may not be shed |
| `flash_write_failed` | integer | flash appends that failed; the entry stayed in RAM |
| `restore_discarded` | integer | bytes of corrupt log tail discarded at boot |
| `flash_bytes` | integer | current on-device log size, bytes |
| `flash_budget` | integer | flash budget after clamping, bytes |

## Payload on `/2/e/`

Little-endian, exactly 90 bytes. Anything else on `PRIVATE_APP` is not a health report.

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | magic, ASCII `TKQH` |
| 4 | 1 | version, `1` |
| 5 | 1 | reserved, `0` |
| 6 | 4 | `uptime_s` |
| 10 | 4 | `capacity` |
| 14 | 4 × 2 | `depth[0..3]`, u16 |
| 22 | 4 × 4 | `enqueued[0..3]` |
| 38 | 4 × 4 | `published[0..3]` |
| 54 | 4 × 4 | `shed[0..3]` |
| 70 | 4 | `p0_refused` |
| 74 | 4 | `flash_write_failed` |
| 78 | 4 | `restore_discarded` |
| 82 | 4 | `flash_bytes` |
| 86 | 4 | `flash_budget` |

## Semantics for the consumer

- Counters are monotonic and survive orderly reboots. After an unclean power loss they can step back to the last persisted value; treat a decrease as a reboot, not as negative traffic.
- `Σ enqueued = Σ published + Σ shed + p0_refused + Σ depth` holds between unclean power losses. It is the device-side denominator RQ1 needs.
- `Σ depth > 0` is the **buffering** device state of REQ-EVT-13.
- Entries published from the queue after an outage are ordinary field packets on the usual topics. They are not marked as replayed; dedup stays on `eventId = sha256(nodeNum:packetId)` (D-006).
