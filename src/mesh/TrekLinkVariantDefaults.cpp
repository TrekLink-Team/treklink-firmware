/*
 * TrekLink factory config defaults.
 *
 * Stock NodeDB declares variantDefaultConfig() weak (src/mesh/NodeDB.cpp:86-87) and calls it at the end of
 * installDefaultConfig() (src/mesh/NodeDB.cpp:800), that is on first boot and after a factory reset. Values set
 * here are ordinary config the owner can change later. No stock variant TrekLink builds on (tbeam,
 * tbeam-s3-core) defines variantDefaultConfig(), so this is the only strong definition in a TrekLink build.
 *
 * Port of fix/relay-ok-to-mqtt (f63c621d6) from the parked overlay layout (treklink/src/VariantDefaults.cpp).
 */
#ifdef TREKLINK_VARIANT

#include "NodeDB.h"

void variantDefaultConfig()
{
    // TrekLink: every packet this unit originates carries the OK-to-MQTT bit (Router.cpp:589). Since Meshtastic
    // 2.7.26 (#9626) a gateway on a broker it treats as public, which includes any broker set by host name,
    // drops other units' packets without that bit (MQTT.cpp:796), relayed SOS included. Stock defaults it to
    // false (NodeDB.cpp:589).
    config.lora.config_ok_to_mqtt = true;
}

#endif // TREKLINK_VARIANT
