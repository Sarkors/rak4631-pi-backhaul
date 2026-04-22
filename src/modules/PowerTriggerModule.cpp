#include "PowerTriggerModule.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "Router.h"

#define BTN_PIN         17      // WB_IO1 → SparkFun BTN (pulse LOW to wake)
#define OFF_PIN         34      // WB_IO2 → SparkFun OFF (drive HIGH to cut power)
#define BTN_PULSE_MS    100     // button press duration in ms
#define KEEPALIVE_MS        (10UL * 60UL * 1000UL)  // 10 minutes
#define KEEPALIVE_ENABLED   false   // set true for production

PowerTriggerModule *powerTriggerModule;

static uint32_t lastHandledId = 0;
static bool piIsOn = false;
static uint32_t keepaliveEndMs = 0;

PowerTriggerModule::PowerTriggerModule()
    : SinglePortModule("PowerTrigger", meshtastic_PortNum_TEXT_MESSAGE_APP),
      concurrency::OSThread("PowerTrigger")
{
    pinMode(BTN_PIN, OUTPUT);
    pinMode(OFF_PIN, OUTPUT);
    digitalWrite(BTN_PIN, HIGH);  // BTN idle = HIGH (not pressed)
    digitalWrite(OFF_PIN, LOW);   // OFF idle = LOW (not cutting power)

    // Force SparkFun output OFF at boot to ensure known state
    digitalWrite(OFF_PIN, HIGH);
    delay(200);
    digitalWrite(OFF_PIN, LOW);
    piIsOn = false;

    LOG_INFO("PowerTrigger: ready — send PI_ON or PI_OFF — Pi forced OFF at boot");
}

void PowerTriggerModule::powerOn()
{
    LOG_INFO("PowerTrigger: powering ON — pulsing BTN LOW for %dms", BTN_PULSE_MS);
    digitalWrite(OFF_PIN, LOW);
    digitalWrite(BTN_PIN, LOW);
    delay(BTN_PULSE_MS);
    digitalWrite(BTN_PIN, HIGH);
    piIsOn = true;
    keepaliveEndMs = millis() + KEEPALIVE_MS;
    LOG_INFO("PowerTrigger: Pi ON — keepalive set for %lu mins", KEEPALIVE_MS / 60000UL);
}

void PowerTriggerModule::powerOff()
{
    LOG_INFO("PowerTrigger: powering OFF — asserting OFF HIGH");
    digitalWrite(BTN_PIN, HIGH);
    digitalWrite(OFF_PIN, HIGH);
    delay(200);                  // hold long enough for SparkFun to cut power
    digitalWrite(OFF_PIN, LOW);  // release OFF pin so BTN can work next time
    piIsOn = false;
    keepaliveEndMs = 0;
    LOG_INFO("PowerTrigger: Pi OFF — OFF pin released");
}

int32_t PowerTriggerModule::runOnce()
{
    if (KEEPALIVE_ENABLED && piIsOn && millis() >= keepaliveEndMs) {
        LOG_INFO("PowerTrigger: keepalive expired — shutting down Pi");
        powerOff();
    }
    return piIsOn ? 5000 : 30000;
}

ProcessMessage PowerTriggerModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (mp.id == lastHandledId) {
        LOG_DEBUG("PowerTrigger: dropping duplicate id=0x%08x", mp.id);
        return ProcessMessage::CONTINUE;
    }

    const size_t len = mp.decoded.payload.size;
    if (len == 0 || len >= meshtastic_Constants_DATA_PAYLOAD_LEN)
        return ProcessMessage::CONTINUE;

    char text[meshtastic_Constants_DATA_PAYLOAD_LEN + 1];
    memcpy(text, mp.decoded.payload.bytes, len);
    text[len] = '\0';

    lastHandledId = mp.id;

    if (strcmp(text, "PI_ON") == 0) {
        if (!piIsOn) {
            powerOn();
        } else {
            LOG_INFO("PowerTrigger: PI_ON ignored — Pi already on, resetting keepalive");
            keepaliveEndMs = millis() + KEEPALIVE_MS;
        }
    }
    else if (strcmp(text, "PI_OFF") == 0) {
        if (piIsOn) {
            powerOff();
        } else {
            LOG_INFO("PowerTrigger: PI_OFF ignored — Pi already off");
        }
    }
    else if (strcmp(text, "PI_RESET") == 0) {
        LOG_INFO("PowerTrigger: PI_RESET — forcing state to OFF, GPIO released");
        digitalWrite(BTN_PIN, HIGH);
        digitalWrite(OFF_PIN, LOW);
        piIsOn = false;
        keepaliveEndMs = 0;
    }
    else {
        LOG_DEBUG("PowerTrigger: no match for '%s'", text);
    }

    return ProcessMessage::CONTINUE;
}