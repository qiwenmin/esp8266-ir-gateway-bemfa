#include <Arduino.h>

#include "DebugLog.h"

#include "ESP8266Boot.h"
#include "bemfa.h"
#include "httpd.h"

#include "hw.h"
#include "bemfa.inc"

#include "panasonic-light-01.h"

#ifdef ENABLE_DEBUG_LOG
Print *DebugLogger = &Serial;
#endif

ESP8266Boot boot;

BemfaMqtt bemfaMqtt(bemfa_mqtt_server, bemfa_mqtt_port, bemfa_mqtt_client_id);

Httpd httpd(80);

static String hostname;

void setup() {
    Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);

    hostname = "XE" + String(ESP.getChipId(), DEC);

    // Init bemfaMqtt
    register_panasonic_light_01_handler(bemfaMqtt, hostname, boot.getLed());

    bemfaMqtt.begin();

    // Init httpd
    httpd.begin();

    // Init boot
    boot.setLed(LED_PIN, HIGH);
    boot.setButton(BTN_PIN);
    boot.setHostname(hostname);
    boot.setOTAPassword(hostname);
    boot.begin();
}

void loop() {
    boot.loop();
    bemfaMqtt.loop();

#ifdef ENABLE_DEBUG_LOG
    static auto last_loop_at = millis();

    auto now = millis();
    auto duration = now - last_loop_at;
    if (duration >= 100) {
        DEBUG_LOG("WARN: loop duration ");
        DEBUG_LOG_LN(duration);
    }
    last_loop_at = now;
#endif // ENABLE_DEBUG_LOG
}
