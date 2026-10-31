/*
 * TrekLink overlay entry point, see TrekLink.h.
 */
#include "TrekLink.h"
#include "Capabilities.h"

#include "NodeDB.h"
#include "Observer.h"
#include "alarm/AlarmPlayer.h"
#include "fall/FallDetectionModule.h"
#include "input/InputBroker.h"
#include "main.h"
#include "modules/CannedMessageModule.h"
#include "queue/TrekLinkMqttOverride.h"
#include "test/TrekLinkTestHooks.h"

#ifdef TREKLINK_SOS_BUTTON_PIN
#include "sos/TrekLinkButtonModule.h"
#else
#include "sos/TrekLinkSOSGesture.h"
#endif

#if defined(TREKLINK_FALL_SENSOR_ICM20948)
#include "fall/sensors/ICM20948FallSensor.h"
#elif defined(TREKLINK_FALL_SENSOR_QMI8658)
#include "fall/sensors/QMI8658FallSensor.h"
#endif

#include <string.h>

#if HAS_SCREEN
#include "mesh/generated/meshtastic/cannedmessages.pb.h"
// Stock defines the canned message store in CannedMessageModule.cpp; no header declares it.
extern meshtastic_CannedMessageModuleConfig cannedMessageModuleConfig;
#endif

namespace treklink
{
namespace
{

// TrekLink's default canned messages (design §4.2 O-02).
const char *const CANNED_MESSAGES = "MEDICAL-HELP|LOST-HELP|SAFE|EVAC|LOW BAT|COMING|FOUND|LOST|"
                                    "OK|NO|CHECK?|STOP|GO|WAIT ME|COME2ME|TURNBACK|MEET AT|REQST|"
                                    "N|S|E|W|R|L|TRAIL|CHKPT|CAMP|DANGR|LO|HI|SPPLY|FD|WTER";
// What stock installDefaultCannedMessageModuleConfig() writes (src/modules/CannedMessageModule.cpp).
const char *const STOCK_CANNED_MESSAGES = "Hi|Bye|Yes|No|Ok";

#if TREKLINK_HAS_FALL_DETECTION
// Any input event cancels a fall pre-alarm (design §4.2 O-07).
class InputCancelObserver
{
  public:
    void attach()
    {
        if (inputBroker)
            observer.observe(inputBroker);
    }

  private:
    int onInput(const InputEvent *event)
    {
        if (fallDetectionModule && fallDetectionModule->isInPreAlarm()) {
            LOG_INFO("TrekLink: input event %d cancels the fall pre-alarm", event ? (int)event->inputEvent : -1);
            fallDetectionModule->cancelFallAlarm();
        }
        return 0; // never consume the event
    }
    CallbackObserver<InputCancelObserver, const InputEvent *> observer =
        CallbackObserver<InputCancelObserver, const InputEvent *>(this, &InputCancelObserver::onInput);
};
InputCancelObserver inputCancelObserver;
#endif

// Owner name "TrekLink xxxx" in place of stock "Meshtastic xxxx" (design §4.2 O-04).
void applyOwnerDefault()
{
    if (strncmp(owner.long_name, "Meshtastic ", 11) != 0)
        return;
    char name[sizeof(owner.long_name)];
    snprintf(name, sizeof(name), "TrekLink %04x", (unsigned)(nodeDB->getNodeNum() & 0x0ffff));
    LOG_INFO("TrekLink: owner name %s replaces %s", name, owner.long_name);
    strncpy(owner.long_name, name, sizeof(owner.long_name));
    nodeDB->saveToDisk(SEGMENT_DEVICESTATE);
}

void applyCannedMessageDefault()
{
#if HAS_SCREEN
    if (!cannedMessageModule)
        return;
    if (strcmp(cannedMessageModuleConfig.messages, STOCK_CANNED_MESSAGES) != 0)
        return;
    LOG_INFO("TrekLink: installing the TrekLink canned messages");
    cannedMessageModule->handleSetCannedMessageModuleMessages(CANNED_MESSAGES);
#endif
}

} // namespace

void start()
{
    LOG_INFO("TrekLink: %s, overlay %s, Meshtastic %s", TREKLINK_VARIANT_NAME, TREKLINK_OVERLAY_VERSION, optstr(APP_VERSION));

    applyOwnerDefault();
    applyCannedMessageDefault();

    alarmPlayer = new AlarmPlayer(); // before the SOS modules, which start and stop it

#ifdef TREKLINK_SOS_BUTTON_PIN
    trekLinkButtonModule = new TrekLinkButtonModule();
    LOG_INFO("TrekLink: SOS on the dedicated button, GPIO %d", TREKLINK_SOS_BUTTON_PIN);
#else
    trekLinkSOSGesture = new TrekLinkSOSGesture();
    LOG_INFO("TrekLink: SOS on the user button, GPIO %d", BUTTON_PIN);
#endif

#if defined(TREKLINK_FALL_SENSOR_ICM20948)
    fallDetectionModule = new FallDetectionModule(new ICM20948FallSensor());
#elif defined(TREKLINK_FALL_SENSOR_QMI8658)
    fallDetectionModule = new FallDetectionModule(new QMI8658FallSensor());
#endif
#if TREKLINK_HAS_FALL_DETECTION
    inputCancelObserver.attach();
    LOG_INFO("TrekLink: fall detection on");
#else
    LOG_INFO("TrekLink: no fall detection on this variant");
#endif

#if defined(TREKLINK_TEST_HOOKS) && TREKLINK_TEST_HOOKS
    new TrekLinkTestHooks();
#endif

#if TREKLINK_OQ_ACTIVE
    trekLinkMqttDrain = new TrekLinkMqttDrain();
    LOG_INFO("TrekLink: onboard MQTT queue on, %u entries", (unsigned)TREKLINK_OQ_MAX_ENTRIES);
#endif
}

} // namespace treklink
