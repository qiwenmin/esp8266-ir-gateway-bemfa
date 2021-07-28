#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <Ticker.h>

#include "DebugLog.h"
#include "panasonic-light-01.h"

#define IR_SWITCH_PIN (14)
#define IR_LED_PIN (13)

static IRsend irsend(IR_LED_PIN);

static String lastMsg;
static bool wsState = false;

Ticker irSendTicker;
static Led *led = 0;

static const uint16_t rawDataLen = 83;
static const uint16_t rawDataOff[rawDataLen] = {
    3492, 1712, 466, 406, 464, 406, 464, 1278,
    466, 1278, 464, 406, 464, 1278, 464, 406,
    464, 406, 464, 406, 464, 1278, 466, 406,
    464, 406, 464, 1278, 464, 406, 464, 1278,
    464, 406, 464, 1278, 464, 406, 464, 406,
    464, 1278, 464, 406, 464, 406, 464, 406,
    464, 406, 466, 1276, 466, 1276, 466, 1278,
    464, 1278, 464, 408, 464, 1278, 464, 406,
    464, 406, 464, 406, 464, 1278, 464, 1278,
    464, 408, 464, 408, 464, 1278, 464, 406,
    464, 406, 464}; // UNKNOWN 1B9C2A92
static const uint16_t rawDataOn[rawDataLen] = {
    3488, 1714, 464, 408, 462, 408, 464, 1278,
    464, 1278, 464, 408, 462, 1278, 464, 408,
    462, 408, 464, 408, 462, 1280, 464, 408,
    462, 408, 462, 1280, 462, 408, 462, 1278,
    464, 408, 462, 1278, 464, 408, 462, 408,
    462, 1278, 464, 408, 462, 408, 462, 408,
    464, 408, 462, 1278, 464, 408, 462, 1278,
    464, 1280, 464, 408, 462, 1280, 464, 408,
    464, 408, 462, 408, 464, 408, 464, 1278,
    464, 408, 462, 408, 462, 1280, 462, 408,
    462, 408, 462}; // UNKNOWN F6B92168
static const uint16_t sendRawInHz = 38000;

static bool switch_light(bool isOn) {
    if (wsState == isOn) {
        return false;
    }

    wsState = isOn;

    if (led) {
        if (isOn) {
            led->on();
        } else {
            led->off();
        }
    }

    // Send 2 times
    irsend.sendRaw(wsState ? rawDataOn : rawDataOff, rawDataLen, sendRawInHz);

    irSendTicker.once_ms(100, []() {
        irsend.sendRaw(wsState ? rawDataOn : rawDataOff, rawDataLen, sendRawInHz);
    });


    return true;
}

void register_panasonic_light_01_handler(BemfaMqtt &bemfaMqtt, const String& topicPrefix, Led *theLed) {
    led = theLed;

    // init hardware
    irsend.begin();

    pinMode(IR_SWITCH_PIN, OUTPUT);
    digitalWrite(IR_SWITCH_PIN, LOW);

    // register mqtt event
    auto topic = topicPrefix + "x002"; // light device
    topic.toLowerCase();

    bemfaMqtt.onMessage(topic, [](const String &topic, const String &msg, AsyncMqttClient &mqttClient) {
        DEBUG_LOG("[LIGHT-01] OnMessage: <");
        DEBUG_LOG(topic);
        DEBUG_LOG(">: ");
        DEBUG_LOG(msg);
        DEBUG_LOG_LN();

        bool changed = false;

        if (msg == "on" || msg.startsWith("on#")) {
            changed = switch_light(true);
        } else if (msg == "off" || msg.startsWith("off#")) {
            changed = switch_light(false);
        }

        if (changed) {
            mqttClient.publish(topic.c_str(), 2, true, msg.c_str(), msg.length());
        }
    });
}
