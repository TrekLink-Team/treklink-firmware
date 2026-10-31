/*
 * TrekLink factory defaults (specs/architecture design §4.2 O-01).
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
}

} // namespace treklink
