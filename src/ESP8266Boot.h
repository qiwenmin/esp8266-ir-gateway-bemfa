#pragma once

#include <Arduino.h>
#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>

#include "DebugPrint.h"

class ESP8266Boot : public DebugPrint {
public:
    static const uint8_t PIN_NONE = 255;

    typedef enum {
        INIT = 0,
        WIFI_CONNECTING,
        READY,
        SMART_CONFIG
    } State;

    ESP8266Boot() :
        _led_pin(PIN_NONE), _led_on_val(HIGH),
        _btn_pin(PIN_NONE), _btn_down_val(LOW), _btn_pin_mode(INPUT_PULLUP),
        _hostname(""), _ota_password(""),
        _state(INIT) {
    };

    void setLed(uint8_t pin, uint8_t on_val = HIGH) {
        _led_pin = pin;
        _led_on_val = on_val;
    };

    void setButton(uint8_t pin, uint8_t down_val = LOW, uint8_t mode = INPUT_PULLUP) {
        _btn_pin = pin;
        _btn_down_val = down_val;
        _btn_pin_mode = mode;
    };

    void setHostname(const String& hostname) {
        _hostname = hostname;
    };

    void setOTAPassword(const String& password) {
        _ota_password = password;
    };

    void begin() {
        this->setupPins();
        this->setupWiFi();
        this->setupOTA();
    };

    void loop() {
        this->_btn_loop();
        ArduinoOTA.handle();
    };

    State getState() {
        return _state;
    };

private:
    uint8_t _led_pin;
    uint8_t _led_on_val;

    uint8_t _btn_pin;
    uint8_t _btn_down_val;
    uint8_t _btn_pin_mode;

    String _hostname;
    String _ota_password;

private:
    void setupPins() {
        if (_led_pin != PIN_NONE) {
            pinMode(_led_pin, OUTPUT);
            digitalWrite(_led_pin, !_led_on_val);

            _led_start();
        }

        if (_btn_pin != PIN_NONE) {
            pinMode(_btn_pin, _btn_pin_mode);
            _btn_hold_since = millis();

            _btn_ticker.attach_ms(50, _btn_ticker_callback, this);
        }
    };

    void _connectWiFi() {
        _led_connecting();
        _state = WIFI_CONNECTING;
        WiFi.begin();
    }

    void setupWiFi() {
        debugPrint()->println();

        WiFi.mode(WIFI_STA);

        if (_hostname.length()) {
            WiFi.setHostname(_hostname.c_str());
        }

        debugPrint()->print("[BOOT] Connecting to WiFi [");
        debugPrint()->print(WiFi.macAddress());
        debugPrint()->println("]...");

        _wifi_connected_handler = WiFi.onStationModeGotIP([this](const WiFiEventStationModeGotIP &) {
            debugPrint()->println("[BOOT] State -> READY");

            _state = READY;

            _led_stop();

            debugPrint()->println("[BOOT] WiFi connected.");
            debugPrint()->print("[BOOT]  SSID: ");
            debugPrint()->println(WiFi.SSID());
            debugPrint()->print("[BOOT]  Hostname: ");
            debugPrint()->println(WiFi.hostname());
            debugPrint()->print("[BOOT]  IP address: ");
            debugPrint()->println(WiFi.localIP());
        });

        _wifi_disconnected_handler = WiFi.onStationModeDisconnected([this](const WiFiEventStationModeDisconnected &) {
            debugPrint()->println("[BOOT] WiFi disconnected.");

            if (_state != READY) return;

            debugPrint()->println("[BOOT] State -> WIFI_CONNECTING");
            _state = WIFI_CONNECTING;

            _led_connecting();
        });

        _connectWiFi();
    };

    void setupOTA() {
        if (_hostname.length()) {
            ArduinoOTA.setHostname(_hostname.c_str());
        }

        if (_ota_password.length()) {
            ArduinoOTA.setPassword(_ota_password.c_str());
        }

        ArduinoOTA.begin();
        debugPrint()->println("[BOOT] Arduino OTA begin...");
        debugPrint()->print("[BOOT]  Hostname: ");
        debugPrint()->println(ArduinoOTA.getHostname());
    };

    void _led_connecting() {
        _led_start();
        _update_led_pattern(0xFF00FF00); // slow flashing
    };

    void _led_smartconfig() {
        _led_start();
        _update_led_pattern(0x05050505); // double flashing
    };

    void _led_reboot() {
        _led_start();
        _update_led_pattern(0xFFFF0000); // very slow flashing
    };

    void _led_start() {
        if (!_led_ticker.active()) {
            _update_led_pattern(0x00); // Off
            _led_ticker.attach_ms(100, _led_ticker_callback, this);
        }
    };

    void _led_stop() {
        _led_ticker.detach();
        _update_led_pattern(0x00); // Off
        _led_ticker_callback(this);
    };

    static void _led_ticker_callback(ESP8266Boot *self) {
        if (self->_led_pin != PIN_NONE) {
            self->_led_pattern_current = (self->_led_pattern_current >> 31) | (self->_led_pattern_current << 1);
            digitalWrite(self->_led_pin, self->_led_pattern_current & 0x01 ? self->_led_on_val : !self->_led_on_val);
        }
    };

    static void _btn_ticker_callback(ESP8266Boot *self) {
        if (self->_btn_pin != PIN_NONE) {
            auto val = digitalRead(self->_btn_pin);
            auto now = millis();

            auto duration = now - self->_btn_hold_since;
            if (duration > 10000) { // Reset
                self->_led_reboot();
                if (val != self->_btn_down_val) {
                    self->debugPrint()->println("[BOOT] Reset...");
                    ESP.restart();
                }
            } else if (duration > 5000) { // smart config
                self->_led_smartconfig();
                if (val != self->_btn_down_val) {
                    // button up
                    self->_state = SMART_CONFIG;
                }
            }
            if (val != self->_btn_down_val) {
                self->_btn_hold_since = millis();
            }
        }
    };

    void _btn_loop() {
        if (_btn_pin != PIN_NONE) {
            if (_state == SMART_CONFIG) {
                debugPrint()->println("[BOOT] Starting SmartConfig...");

                WiFi.stopSmartConfig();

                WiFi.mode(WIFI_STA);
                WiFi.beginSmartConfig();
                while (1) {
                    if (WiFi.smartConfigDone()) {
                        debugPrint()->println("[BOOT] Done");
                        debugPrint()->printf("SSID: %s\n", WiFi.SSID().c_str());

                        WiFi.setAutoConnect(true);

                        _connectWiFi();

                        break;
                    }

                    delay(100);
                }
            }
        }
    };

    void _update_led_pattern(uint32_t pattern) {
        if (_led_pattern != pattern) {
            _led_pattern_current = _led_pattern = pattern;
        }
    };

private:
    uint32_t _led_pattern;
    uint32_t _led_pattern_current;
    Ticker _led_ticker;

    unsigned long _btn_hold_since;
    Ticker _btn_ticker;

    State _state;

    WiFiEventHandler _wifi_connected_handler;
    WiFiEventHandler _wifi_disconnected_handler;
};