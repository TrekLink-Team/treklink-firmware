/*
 * TrekLink MQTT override, see TrekLinkMqttOverride.h.
 *
 * The queued path mirrors the gating and encoding of stock MQTT::onSend() at v2.7.26.54e0d8d
 * (src/mqtt/MQTT.cpp, the body of MQTT::onSend). The overlay check hashes that function, so an upstream
 * change to it is reviewed before a sync lands (specs/architecture design §4.3).
 */
#include "queue/TrekLinkMqttOverride.h"

#if TREKLINK_OQ_ACTIVE

#include "MeshTypes.h"
#include "NodeDB.h"
#include "RTC.h"
#include "mesh/Channels.h"
#include "mesh/Router.h"
#include "mesh/generated/meshtastic/mqtt.pb.h"
#include "mqtt/MQTT.h"
#include "mqtt/ServiceEnvelope.h"
#include "serialization/MeshPacketSerializer.h"
#include <IPAddress.h>
#include <string>

TrekLinkMqttDrain *trekLinkMqttDrain = nullptr;

namespace
{

uint8_t envBytes[meshtastic_MqttClientProxyMessage_size + 30]; // as stock MQTT.cpp `bytes`

std::string topicRoot()
{
    return *moduleConfig.mqtt.root ? std::string(moduleConfig.mqtt.root) : std::string("msh");
}

// Stock learns whether the broker is private from the resolved address; TrekLink can only read the
// configured one, so a host name counts as public, which uplinks less, never more, than stock.
bool brokerAddressIsPrivate()
{
    IPAddress ip;
    std::string host(moduleConfig.mqtt.address);
    size_t colon = host.find(':');
    if (colon != std::string::npos)
        host = host.substr(0, colon);
    if (!ip.fromString(host.c_str()))
        return false;
    // The private ranges stock isPrivateIpAddress() lists (src/mqtt/MQTT.cpp), which is not linkable here.
    const uint8_t a = ip[0], b = ip[1];
    return a == 10 || (a == 127 && b == 0 && ip[2] == 0 && ip[3] == 1) || (a == 172 && b >= 16 && b <= 31) || (a == 192 && b == 168) || (a == 100 && b >= 64 && b <= 127) ||
           (a == 169 && b == 254);
}

bool linkUp()
{
    return mqtt && (moduleConfig.mqtt.proxy_to_client_enabled || mqtt->isConnectedDirectly());
}

// Same rules and the same envelope as stock MQTT::onSend(); returns false when stock would not uplink.
bool encodeLikeStock(const meshtastic_MeshPacket &mpEncrypted, const meshtastic_MeshPacket &mpDecoded, ChannelIndex chIndex,
                     std::string &topic, size_t &numBytes)
{
    if (mpEncrypted.via_mqtt)
        return false;
    bool uplinkEnabled = false;
    for (int i = 0; i <= 7; i++) {
        if (channels.getByIndex(i).settings.uplink_enabled)
            uplinkEnabled = true;
    }
    if (!uplinkEnabled)
        return false;
    auto &ch = channels.getByIndex(chIndex);

    if (mpDecoded.which_payload_variant == meshtastic_MeshPacket_decoded_tag) {
        bool dontUplink = !mpDecoded.decoded.has_bitfield || !(mpDecoded.decoded.bitfield & BITFIELD_OK_TO_MQTT_MASK);
        if (!isFromUs(&mpDecoded) && !brokerAddressIsPrivate() && dontUplink)
            return false;
        if (mqtt->isUsingDefaultServer() && (mpDecoded.decoded.portnum == meshtastic_PortNum_RANGE_TEST_APP ||
                                             mpDecoded.decoded.portnum == meshtastic_PortNum_DETECTION_SENSOR_APP))
            return false;
    }
    bool isPKIEncrypted = mpEncrypted.pki_encrypted || mpDecoded.pki_encrypted;
    if (!(ch.settings.uplink_enabled || isPKIEncrypted))
        return false;
    const char *channelId = isPKIEncrypted ? "PKI" : channels.getGlobalId(chIndex);

    const meshtastic_MeshPacket *p;
    if (moduleConfig.mqtt.encryption_enabled)
        p = &mpEncrypted;
    else if (mpDecoded.which_payload_variant == meshtastic_MeshPacket_decoded_tag)
        p = &mpDecoded;
    else
        return false;

    std::string nodeId = nodeDB->getNodeId();
    const meshtastic_ServiceEnvelope env = {.packet = const_cast<meshtastic_MeshPacket *>(p),
                                            .channel_id = const_cast<char *>(channelId),
                                            .gateway_id = const_cast<char *>(nodeId.c_str())};
    numBytes = pb_encode_to_bytes(envBytes, sizeof(envBytes), &meshtastic_ServiceEnvelope_msg, &env);
    topic = topicRoot() + "/2/e/" + channelId + "/" + nodeId;
    return true;
}

void publishJsonCompanion(const meshtastic_MeshPacket *packet, const char *channelId)
{
#if !defined(ARCH_NRF52) || defined(NRF52_USE_JSON)
    if (!moduleConfig.mqtt.json_enabled)
        return;
    auto jsonString = MeshPacketSerializer::JsonSerialize(packet);
    if (jsonString.length() == 0)
        return;
    std::string nodeId = nodeDB->getNodeId();
    std::string topicJson = topicRoot() + "/2/json/" + (packet->pki_encrypted ? "PKI" : channelId) + "/" + nodeId;
    mqtt->publish(topicJson.c_str(), jsonString.c_str(), false);
#endif
}

template <typename T> std::string jsonArray(const T *v)
{
    std::string s = "[";
    for (uint8_t t = 0; t < treklink::oq::TIER_COUNT; t++) {
        if (t)
            s += ",";
        s += std::to_string((unsigned long)v[t]);
    }
    return s + "]";
}

} // namespace

// The real stock function and its wrapper (-Wl,--wrap=_ZN4MQTT6onSendERK22_meshtastic_MeshPacketS2_h).
extern "C" void __real__ZN4MQTT6onSendERK22_meshtastic_MeshPacketS2_h(MQTT *self, const meshtastic_MeshPacket &mpEncrypted,
                                                                       const meshtastic_MeshPacket &mpDecoded,
                                                                       ChannelIndex chIndex);

extern "C" void __wrap__ZN4MQTT6onSendERK22_meshtastic_MeshPacketS2_h(MQTT *self, const meshtastic_MeshPacket &mpEncrypted,
                                                                       const meshtastic_MeshPacket &mpDecoded,
                                                                       ChannelIndex chIndex)
{
    // specs/onboard-queue REQ-EVT-01/02: the stock fast path only while no backlog exists, so live
    // traffic cannot overtake queued SOS traffic.
    if (!trekLinkMqttDrain || (linkUp() && trekLinkMqttDrain->queue.empty())) {
        __real__ZN4MQTT6onSendERK22_meshtastic_MeshPacketS2_h(self, mpEncrypted, mpDecoded, chIndex);
        return;
    }
    std::string topic;
    size_t numBytes = 0;
    if (!encodeLikeStock(mpEncrypted, mpDecoded, chIndex, topic, numBytes))
        return;
    LOG_INFO("TrekLinkQueue: link down or backlog, queue packet id=0x%08x", (unsigned)mpDecoded.id);
    trekLinkMqttDrain->queue.enqueue(mpDecoded, topic, envBytes, numBytes);
}

TrekLinkMqttDrain::TrekLinkMqttDrain() : concurrency::OSThread("TrekLinkMqttDrain") {}

int32_t TrekLinkMqttDrain::runOnce()
{
    treklink::oq::QueueCore &q = queue.core();
    const bool up = linkUp();

    if (!up) {
        if (linkWasUp) {
            if (q.hasInFlight())
                q.release(q.inFlight()); // not confirmed while the link held: publish it again (REQ-EVT-10)
            queue.linkLost();
        }
        linkWasUp = false;
        return 1000;
    }
    linkWasUp = true;

    // An entry published on an earlier tick is deleted once the link is still up a tick later, the nearest
    // observable stand-in for stock's successful poll (REQ-EVT-10).
    if (q.hasInFlight())
        q.commit(q.inFlight());

    publishHealth();
    if (q.empty())
        return 200;

    const uint32_t now = millis();
    if (queue.lastDrainMs != 0 && now - queue.lastDrainMs < TREKLINK_OQ_DRAIN_INTERVAL_MS)
        return 50; // REQ-STA-03

    treklink::oq::Pending p;
    if (!q.peek(p))
        return 200;
    LOG_INFO("TrekLinkQueue: publish P%u id=0x%08x, %u bytes to %s, %u left", (unsigned)p.tier, (unsigned)p.packetId,
             (unsigned)p.env.size(), p.topic.c_str(), (unsigned)(q.depth() - 1));
    if (!mqtt->publish(p.topic.c_str(), p.env.data(), p.env.size(), false))
        return 200;
    queue.lastDrainMs = now ? now : 1;
    q.markInFlight(p.seq);
    if (moduleConfig.mqtt.proxy_to_client_enabled)
        q.commit(p.seq); // handed to the phone, as stock treats it

    // JSON companion, as stock publishQueuedMessages() builds it; its failure never blocks deletion.
    const DecodedServiceEnvelope env(p.env.data(), p.env.size());
    if (env.validDecode && env.packet != NULL && env.channel_id != NULL)
        publishJsonCompanion(env.packet, env.channel_id);
    return 50;
}

// MQTT-only queue health report on PRIVATE_APP (specs/onboard-queue REQ-EVT-12): never sent over LoRa,
// never queued. Stock JSON serialisation drops PRIVATE_APP payloads, so the JSON form is built here.
void TrekLinkMqttDrain::publishHealth()
{
    const uint32_t now = millis();
    if (!queue.healthDue(now))
        return;
    if (healthAttempted && now - lastHealthAttemptMs < TREKLINK_OQ_HEALTH_RETRY_S * 1000UL)
        return;
    healthAttempted = true;
    lastHealthAttemptMs = now;
    const auto &ch = channels.getByIndex(0);
    if (!ch.settings.uplink_enabled)
        return;

    meshtastic_MeshPacket pkt = meshtastic_MeshPacket_init_zero;
    pkt.from = nodeDB->getNodeNum();
    pkt.to = NODENUM_BROADCAST;
    pkt.id = generatePacketId();
    pkt.channel = 0;
    pkt.rx_time = getValidTime(RTCQualityFromNet);
    pkt.which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    pkt.decoded.portnum = meshtastic_PortNum_PRIVATE_APP;
    std::vector<uint8_t> payload;
    queue.healthPayload(now / 1000, payload);
    if (payload.size() > sizeof(pkt.decoded.payload.bytes))
        return;
    memcpy(pkt.decoded.payload.bytes, payload.data(), payload.size());
    pkt.decoded.payload.size = payload.size();

    const char *channelId = channels.getGlobalId(0);
    std::string nodeId = nodeDB->getNodeId();
    const meshtastic_ServiceEnvelope env = {
        .packet = &pkt, .channel_id = const_cast<char *>(channelId), .gateway_id = const_cast<char *>(nodeId.c_str())};
    size_t numBytes = pb_encode_to_bytes(envBytes, sizeof(envBytes), &meshtastic_ServiceEnvelope_msg, &env);
    std::string topic = topicRoot() + "/2/e/" + channelId + "/" + nodeId;
    if (!mqtt->publish(topic.c_str(), envBytes, numBytes, false))
        return;
    LOG_INFO("TrekLinkQueue: health report published to %s", topic.c_str());

#if !defined(ARCH_NRF52) || defined(NRF52_USE_JSON)
    if (moduleConfig.mqtt.json_enabled) {
        treklink::oq::Health h;
        if (treklink::oq::decodeHealth(payload.data(), payload.size(), h)) {
            char head[200];
            snprintf(head, sizeof(head),
                     "{\"id\":%u,\"timestamp\":%u,\"to\":%u,\"from\":%u,\"channel\":0,\"type\":\"treklink_queue_health\","
                     "\"sender\":\"%s\",\"payload\":{",
                     (unsigned)pkt.id, (unsigned)pkt.rx_time, (unsigned)pkt.to, (unsigned)pkt.from, nodeId.c_str());
            std::string json = head;
            json += "\"schema\":\"treklink.queue_health\",\"v\":" + std::to_string((unsigned)treklink::oq::HEALTH_VERSION);
            json += ",\"uptime_s\":" + std::to_string((unsigned long)h.uptimeS);
            json += ",\"capacity\":" + std::to_string((unsigned long)h.capacity);
            json += ",\"depth\":" + jsonArray(h.depth) + ",\"enqueued\":" + jsonArray(h.enqueued) +
                    ",\"published\":" + jsonArray(h.published) + ",\"shed\":" + jsonArray(h.shed);
            json += ",\"p0_refused\":" + std::to_string((unsigned long)h.p0Refused);
            json += ",\"flash_write_failed\":" + std::to_string((unsigned long)h.flashWriteFailed);
            json += ",\"restore_discarded\":" + std::to_string((unsigned long)h.restoreDiscarded);
            json += ",\"flash_bytes\":" + std::to_string((unsigned long)h.flashBytes);
            json += ",\"flash_budget\":" + std::to_string((unsigned long)h.flashBudget) + "}}";
            std::string topicJson = topicRoot() + "/2/json/" + channelId + "/" + nodeId;
            mqtt->publish(topicJson.c_str(), json.c_str(), false);
        }
    }
#endif
    queue.healthSent(now);
    healthAttempted = false;
}

#endif // TREKLINK_OQ_ACTIVE
