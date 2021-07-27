#include <Arduino.h>

#include "ESP8266Boot.h"
#include "bemfa.h"

#include "hw.h"
#include "bemfa.inc"

#include "panasonic-light-01.h"

ESP8266Boot boot;

BemfaMqtt bemfaMqtt(bemfa_mqtt_server, bemfa_mqtt_port, bemfa_mqtt_client_id);

static String hostname;

void setup() {
    Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);

    hostname = "XE" + String(ESP.getChipId(), DEC);

#ifdef DEV_BOARD
    bemfaMqtt.setDebugPrint(&Serial);
    boot.setDebugPrint(&Serial);
#endif // DEV_BOARD

    // Init bemfaMqtt
    register_panasonic_light_01_handler(bemfaMqtt, hostname);

    bemfaMqtt.begin();

    // Init boot
    boot.setLed(LED_PIN, LOW);
    boot.setButton(BTN_PIN);
    boot.setHostname(hostname);
    boot.setOTAPassword(hostname);
    boot.begin();
}

void loop() {
    boot.loop();
    bemfaMqtt.loop();

#ifdef DEV_BOARD
    static auto last_loop_at = millis();

    auto now = millis();
    auto duration = now - last_loop_at;
    if (duration >= 100) {
        Serial.print("WARN: loop duration ");
        Serial.println(duration);
    }
    last_loop_at = now;
#endif // DEV_BOARD
}
