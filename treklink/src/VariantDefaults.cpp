/*
 * TrekLink factory defaults (specs/architecture design §4.2 O-01, O-13).
 *
 * Stock NodeDB declares variantDefaultConfig() weak (src/mesh/NodeDB.cpp:86-87) and calls it at the end
 * of installDefaultConfig() (src/mesh/NodeDB.cpp:800), that is on first boot and after a factory reset.
 * Values set here are ordinary config the owner can change later.
 */
#include "Capabilities.h"
#include "NodeDB.h"
#include "TrekLink.h"

#include <string.h>

void variantDefaultConfig()
{
    treklink::applyConfigDefaults();
}

namespace treklink
{

void applyConfigDefaults()
{
    // Vietnam, UTC+7, no daylight saving; stock main.cpp uses tzdef when it is set (src/main.cpp:866-875).
    strncpy(config.device.tzdef, "ICT-7", sizeof(config.device.tzdef));
    // Every packet this unit originates carries OK-to-MQTT (Router.cpp:589), so a gateway uplinks a relayed
    // TrekLink SOS for any broker, also from its queue, which cannot tell a private host name (O-13).
    config.lora.config_ok_to_mqtt = true;
}

} // namespace treklink
