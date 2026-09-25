#include "../test_helpers.h"
#include "mqtt/TrekLinkQueueCore.h"
#include <memory>

// PRIVATE_APP (256): the TrekLink onboard-queue health report is expanded into JSON; every other
// PRIVATE_APP payload must serialise exactly as stock (specs/onboard-queue REQ-EVT-12, REQ-UBI-03).

static uint32_t number_at(JSONObject &obj, const char *key)
{
    auto it = obj.find(key);
    TEST_ASSERT_TRUE_MESSAGE(it != obj.end(), key);
    return (uint32_t)it->second->AsNumber();
}

void test_private_app_queue_health_serialization()
{
    treklink::oq::Health h;
    memset(&h, 0, sizeof(h));
    h.uptimeS = 5321;
    h.capacity = 200;
    for (uint8_t t = 0; t < treklink::oq::TIER_COUNT; t++) {
        h.depth[t] = t + 1;
        h.enqueued[t] = 100 + t;
        h.published[t] = 50 + t;
        h.shed[t] = t == 0 ? 0 : 10 + t;
    }
    h.p0Refused = 1;
    h.flashWriteFailed = 2;
    h.restoreDiscarded = 3;
    h.flashBytes = 4096;
    h.flashBudget = 262144;
    std::vector<uint8_t> wire;
    treklink::oq::encodeHealth(h, wire);

    meshtastic_MeshPacket packet = create_test_packet(meshtastic_PortNum_PRIVATE_APP, wire.data(), wire.size());
    std::string json = MeshPacketSerializer::JsonSerialize(&packet, false);
    std::unique_ptr<JSONValue> root(JSON::Parse(json.c_str()));
    TEST_ASSERT_NOT_NULL(root.get());
    JSONObject obj = root->AsObject();

    TEST_ASSERT_EQUAL_STRING("treklink_queue_health", obj["type"]->AsString().c_str());
    TEST_ASSERT_TRUE(obj.find("payload") != obj.end());
    JSONObject payload = obj["payload"]->AsObject();
    TEST_ASSERT_EQUAL_STRING("treklink.queue_health", payload["schema"]->AsString().c_str());
    TEST_ASSERT_EQUAL_UINT32(1, number_at(payload, "v"));
    TEST_ASSERT_EQUAL_UINT32(5321, number_at(payload, "uptime_s"));
    TEST_ASSERT_EQUAL_UINT32(200, number_at(payload, "capacity"));
    TEST_ASSERT_EQUAL_UINT32(1, number_at(payload, "p0_refused"));
    TEST_ASSERT_EQUAL_UINT32(2, number_at(payload, "flash_write_failed"));
    TEST_ASSERT_EQUAL_UINT32(3, number_at(payload, "restore_discarded"));
    TEST_ASSERT_EQUAL_UINT32(4096, number_at(payload, "flash_bytes"));
    TEST_ASSERT_EQUAL_UINT32(262144, number_at(payload, "flash_budget"));

    const char *arrays[] = {"depth", "enqueued", "published", "shed"};
    for (int a = 0; a < 4; a++) {
        JSONArray arr = payload[arrays[a]]->AsArray();
        TEST_ASSERT_EQUAL_MESSAGE(4, arr.size(), arrays[a]);
    }
    JSONArray depth = payload["depth"]->AsArray();
    TEST_ASSERT_EQUAL_UINT32(4, (uint32_t)depth[3]->AsNumber());
    JSONArray shed = payload["shed"]->AsArray();
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)shed[0]->AsNumber());

    // Envelope fields are untouched.
    TEST_ASSERT_EQUAL_UINT32(0x9999, number_at(obj, "id"));
    TEST_ASSERT_EQUAL_UINT32(0x11223344, number_at(obj, "from"));
}

void test_private_app_foreign_payload_is_stock()
{
    // Stock output for an unknown PortNum: envelope fields, empty type, no payload key.
    const char *other = "{\"not\":\"ours\"}";
    meshtastic_MeshPacket packet =
        create_test_packet(meshtastic_PortNum_PRIVATE_APP, reinterpret_cast<const uint8_t *>(other), strlen(other));
    std::string json = MeshPacketSerializer::JsonSerialize(&packet, false);
    std::unique_ptr<JSONValue> root(JSON::Parse(json.c_str()));
    TEST_ASSERT_NOT_NULL(root.get());
    JSONObject obj = root->AsObject();
    TEST_ASSERT_EQUAL_STRING("", obj["type"]->AsString().c_str());
    TEST_ASSERT_TRUE(obj.find("payload") == obj.end());

    // Same for a truncated health record: it must not be half-decoded.
    treklink::oq::Health h;
    memset(&h, 0, sizeof(h));
    std::vector<uint8_t> wire;
    treklink::oq::encodeHealth(h, wire);
    meshtastic_MeshPacket cut = create_test_packet(meshtastic_PortNum_PRIVATE_APP, wire.data(), wire.size() - 1);
    std::string json2 = MeshPacketSerializer::JsonSerialize(&cut, false);
    std::unique_ptr<JSONValue> root2(JSON::Parse(json2.c_str()));
    JSONObject obj2 = root2->AsObject();
    TEST_ASSERT_TRUE(obj2.find("payload") == obj2.end());
}
