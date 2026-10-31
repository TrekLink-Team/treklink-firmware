// TrekLink: unit tests for the channel 0 position precision that SOS positions depend on.
//
// Since Meshtastic 2.7.26, Router::send passes every position packet the node originates through
// applyPositionPrecisionForChannel(*p, p->channel) (Router.cpp, originator only). TrekLink SOS positions
// (TrekLinkSOSHelper::sendPositionPacket and the SOS beacons through PositionModule::sendOurPosition) go out on
// channel 0, so TrekLink variants default channel 0 to 32 bits (USERPREFS_CHANNEL_0_PRECISION in each TrekLink
// variant.h). These tests run that exact clamp on channel 0.
//
// Run: pio test -e native -f test_treklink_position_precision

#include "Channels.h"
#include "NodeDB.h"
#include "PositionPrecision.h"
#include "TestUtil.h"
#include "mesh-pb-constants.h"
#include <stdlib.h>
#include <unity.h>

static const int32_t SOS_LAT_I = 210285123;  // 21.0285123 N
static const int32_t SOS_LON_I = 1058542456; // 105.8542456 E
static const uint32_t TREKLINK_CHANNEL_0_PRECISION = 32;
static const uint32_t STOCK_CHANNEL_0_PRECISION = 13; // Channels.cpp initDefaultChannel

static void setChannel0Precision(uint32_t precision)
{
    channelFile.channels_count = MAX_NUM_CHANNELS;
    meshtastic_Channel &ch = channels.getByIndex(0);
    ch = meshtastic_Channel_init_default;
    ch.has_settings = true;
    ch.role = meshtastic_Channel_Role_PRIMARY;
    ch.settings.has_module_settings = true;
    ch.settings.module_settings.position_precision = precision;
}

// A position packet on channel 0 at priority MAX, as the TrekLink SOS path builds it, with the coordinate
// presence flags set as PositionModule::sendOurPosition sets them.
static meshtastic_MeshPacket makeSOSPositionPacket()
{
    meshtastic_Position pos = meshtastic_Position_init_default;
    pos.has_latitude_i = true;
    pos.latitude_i = SOS_LAT_I;
    pos.has_longitude_i = true;
    pos.longitude_i = SOS_LON_I;
    pos.has_altitude = true;
    pos.altitude = 1143;
    pos.time = 1793500000;

    meshtastic_MeshPacket packet = meshtastic_MeshPacket_init_default;
    packet.channel = 0;
    packet.priority = meshtastic_MeshPacket_Priority_MAX;
    packet.which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    packet.decoded.portnum = meshtastic_PortNum_POSITION_APP;
    packet.decoded.payload.size =
        pb_encode_to_bytes(packet.decoded.payload.bytes, sizeof(packet.decoded.payload.bytes), &meshtastic_Position_msg, &pos);
    return packet;
}

static meshtastic_Position decodePosition(const meshtastic_MeshPacket &packet)
{
    meshtastic_Position decoded = meshtastic_Position_init_default;
    TEST_ASSERT_TRUE(
        pb_decode_from_bytes(packet.decoded.payload.bytes, packet.decoded.payload.size, &meshtastic_Position_msg, &decoded));
    return decoded;
}

static void test_sosPosition_keepsFullPrecisionOnTrekLinkChannel0()
{
    setChannel0Precision(TREKLINK_CHANNEL_0_PRECISION);
    meshtastic_MeshPacket packet = makeSOSPositionPacket();

    TEST_ASSERT_TRUE(applyPositionPrecisionForChannel(packet, packet.channel));

    meshtastic_Position decoded = decodePosition(packet);
    TEST_ASSERT_TRUE(decoded.has_latitude_i);
    TEST_ASSERT_TRUE(decoded.has_longitude_i);
    TEST_ASSERT_EQUAL_INT32(SOS_LAT_I, decoded.latitude_i);
    TEST_ASSERT_EQUAL_INT32(SOS_LON_I, decoded.longitude_i);
    TEST_ASSERT_EQUAL_INT32(1143, decoded.altitude);
    TEST_ASSERT_EQUAL_UINT32(32, decoded.precision_bits);
}

// Documents the regression the TrekLink default avoids: the stock 13-bit default moves the SOS fix to the
// centre of a bucket 2^19 units (about 0.05 degrees) wide.
static void test_sosPosition_isCoarsenedByStockChannel0Default()
{
    setChannel0Precision(STOCK_CHANNEL_0_PRECISION);
    meshtastic_MeshPacket packet = makeSOSPositionPacket();

    TEST_ASSERT_TRUE(applyPositionPrecisionForChannel(packet, packet.channel));

    meshtastic_Position decoded = decodePosition(packet);
    TEST_ASSERT_EQUAL_UINT32(13, decoded.precision_bits);
    TEST_ASSERT_NOT_EQUAL(SOS_LAT_I, decoded.latitude_i);
    TEST_ASSERT_NOT_EQUAL(SOS_LON_I, decoded.longitude_i);
    TEST_ASSERT_INT32_WITHIN(1 << 18, SOS_LAT_I, decoded.latitude_i);
    TEST_ASSERT_INT32_WITHIN(1 << 18, SOS_LON_I, decoded.longitude_i);
}

// Pins the encoding rule behind the TrekLinkSOSHelper::sendPositionPacket fix: a Position whose coordinates are
// assigned without their has_ flags goes on the wire with no coordinates at all, only the time.
static void test_sosPosition_withoutPresenceFlagsCarriesNoCoordinates()
{
    meshtastic_Position pos = meshtastic_Position_init_default;
    pos.latitude_i = SOS_LAT_I;
    pos.longitude_i = SOS_LON_I;
    pos.altitude = 1143;
    pos.time = 1793500000;

    meshtastic_MeshPacket packet = meshtastic_MeshPacket_init_default;
    packet.which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    packet.decoded.portnum = meshtastic_PortNum_POSITION_APP;
    packet.decoded.payload.size =
        pb_encode_to_bytes(packet.decoded.payload.bytes, sizeof(packet.decoded.payload.bytes), &meshtastic_Position_msg, &pos);

    meshtastic_Position decoded = decodePosition(packet);
    TEST_ASSERT_FALSE(decoded.has_latitude_i);
    TEST_ASSERT_FALSE(decoded.has_longitude_i);
    TEST_ASSERT_FALSE(decoded.has_altitude);
    TEST_ASSERT_EQUAL_INT32(0, decoded.latitude_i);
    TEST_ASSERT_EQUAL_INT32(0, decoded.longitude_i);
    TEST_ASSERT_EQUAL_UINT32(1793500000, decoded.time);
}

void setUp(void) {}

void tearDown(void) {}

extern "C" {
void setup()
{
    initializeTestEnvironment();
    UNITY_BEGIN();
    RUN_TEST(test_sosPosition_keepsFullPrecisionOnTrekLinkChannel0);
    RUN_TEST(test_sosPosition_isCoarsenedByStockChannel0Default);
    RUN_TEST(test_sosPosition_withoutPresenceFlagsCarriesNoCoordinates);
    exit(UNITY_END());
}

void loop() {}
}
