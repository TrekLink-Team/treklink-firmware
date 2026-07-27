/*
 * TrekLink SOS Helper Implementation
 * Centralized SOS logic shared across all TrekLink variants.
 */

#include "TrekLinkSOSHelper.h"

#ifdef TREKLINK_VARIANT

#include "BuzzerManager.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "Router.h"
#include "configuration.h"
#include "main.h"

#if HAS_SCREEN
#include "graphics/Screen.h"
#endif

#if !MESHTASTIC_EXCLUDE_GPS
#include "modules/PositionModule.h"
#endif

TrekLinkSOSHelper &TrekLinkSOSHelper::instance()
{
    static TrekLinkSOSHelper inst;
    return inst;
}

// ============================================================
//  Core SOS Actions
// ============================================================

void TrekLinkSOSHelper::triggerSOS(uint8_t buzzerOwner)
{
    LOG_WARN("TrekLinkSOS: SOS TRIGGERED (owner=%d)", buzzerOwner);

    // 1. Send emergency position packet
    sendPositionPacket();

    // 2. Send SOS text message with coordinates
    sendSOSTextMessage("SOS");

    // 3. Activate local alarms
    activateAlarms(buzzerOwner);
}

void TrekLinkSOSHelper::cancelSOS(uint8_t buzzerOwner)
{
    LOG_INFO("TrekLinkSOS: SOS CANCELLED (owner=%d)", buzzerOwner);
    deactivateAlarms(buzzerOwner);
}

void TrekLinkSOSHelper::broadcastPosition()
{
    LOG_INFO("TrekLinkSOS: Broadcasting position (ping)");

#if !MESHTASTIC_EXCLUDE_GPS
    if (positionModule) {
        positionModule->sendOurPosition();
    }
#endif
}

void TrekLinkSOSHelper::activateAlarms(uint8_t buzzerOwner)
{
    LOG_INFO("TrekLinkSOS: Activating alarms (owner=%d)", buzzerOwner);

#ifdef PIN_BUZZER
    BuzzerManager::instance().acquire(static_cast<BuzzerOwner>(buzzerOwner));
    BuzzerManager::instance().write(128); // 50% duty cycle
#endif

#ifdef PIN_VIBRATOR
    digitalWrite(PIN_VIBRATOR, HIGH);
#endif
}

void TrekLinkSOSHelper::deactivateAlarms(uint8_t buzzerOwner)
{
    LOG_INFO("TrekLinkSOS: Deactivating alarms (owner=%d)", buzzerOwner);

#ifdef PIN_BUZZER
    BuzzerManager::instance().write(0);
    BuzzerManager::instance().release(static_cast<BuzzerOwner>(buzzerOwner));
#endif

#ifdef PIN_VIBRATOR
    digitalWrite(PIN_VIBRATOR, LOW);
#endif

#ifdef LED_PIN
    digitalWrite(LED_PIN, LOW);
#endif
}

// ============================================================
//  Position & Text Packets
// ============================================================

void TrekLinkSOSHelper::sendPositionPacket()
{
    // Build position from current node data
    meshtastic_Position pos = meshtastic_Position_init_default;
    meshtastic_NodeInfoLite *node =
        nodeDB->getNodeNum() ? nodeDB->getMeshNode(nodeDB->getNodeNum()) : nullptr;

    if (node && nodeDB->hasValidPosition(node)) {
        pos.latitude_i = node->position.latitude_i;
        pos.longitude_i = node->position.longitude_i;
        pos.altitude = node->position.altitude;
        pos.time = node->position.time;
    }

    // Allocate and configure high-priority position packet
    meshtastic_MeshPacket *packet = router->allocForSending();
    packet->channel = 0;
    packet->priority = meshtastic_MeshPacket_Priority_MAX;
    packet->want_ack = false;
    packet->decoded.portnum = meshtastic_PortNum_POSITION_APP;

    packet->decoded.payload.size = pb_encode_to_bytes(
        packet->decoded.payload.bytes,
        sizeof(packet->decoded.payload.bytes),
        &meshtastic_Position_msg,
        &pos);

    if (service) {
        service->sendToMesh(packet);
    }
}

void TrekLinkSOSHelper::sendSOSTextMessage(const char *prefix)
{
    // Extract GPS coordinates
    meshtastic_NodeInfoLite *node =
        nodeDB->getNodeNum() ? nodeDB->getMeshNode(nodeDB->getNodeNum()) : nullptr;

    char message[100];
    size_t msgLen;

    if (node && nodeDB->hasValidPosition(node)) {
        float lat = node->position.latitude_i * 1e-7;
        float lon = node->position.longitude_i * 1e-7;
        msgLen = snprintf(message, sizeof(message), "%s - [%.6f], [%.6f]", prefix, lat, lon);
    } else {
        msgLen = snprintf(message, sizeof(message), "%s - [No GPS]", prefix);
    }

    if (msgLen >= sizeof(message)) {
        msgLen = sizeof(message) - 1;
    }

    // Send as high-priority text message
    meshtastic_MeshPacket *packet = router->allocForSending();
    packet->channel = 0;
    packet->priority = meshtastic_MeshPacket_Priority_MAX;
    packet->want_ack = false;
    packet->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;

    memcpy(packet->decoded.payload.bytes, message, msgLen);
    packet->decoded.payload.size = msgLen;

    if (service) {
        service->sendToMesh(packet);
    }

    LOG_INFO("TrekLinkSOS: Sent text: %s", message);
}

// ============================================================
//  Beacon Retransmission
// ============================================================

bool TrekLinkSOSHelper::tickBeacon(uint32_t sosStartTime, uint32_t &lastTxTime)
{
    uint32_t now = millis();
    uint32_t elapsed = now - sosStartTime;
    // 5s first minute, then 30s
    uint32_t interval = (elapsed < 60000) ? 5000 : 30000;

    if ((now - lastTxTime) >= interval) {
        LOG_INFO("TrekLinkSOS: Beacon retransmit");
        broadcastPosition();
        lastTxTime = now;
        return true;
    }
    return false;
}

// ============================================================
//  Display
// ============================================================

void TrekLinkSOSHelper::showBanner(const char *message, uint32_t durationMs)
{
#if HAS_SCREEN
    if (screen) {
        screen->showSimpleBanner(message, durationMs);
    }
#endif
}

#endif // TREKLINK_VARIANT
