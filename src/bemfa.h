#pragma once

#include <Arduino.h>
#include <AsyncMqttClient.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <map>
#include <list>
#include <functional>
#include "DebugPrint.h"

class BemfaMqtt : public DebugPrint {
public:
    typedef std::function<void(const String& topic, const String& msg, AsyncMqttClient &mqttClient)> MessageListener;

    BemfaMqtt(const String& host, int port, const String& client_id)
        : _host(host), _port(port), _client_id(client_id) {
    };

    void onMessage(const String& topic, MessageListener listener) {
        _mlsm[topic].push_back(listener);
    };

    void begin() {
        _mqtt_client.setServer(_host.c_str(), _port);
        _mqtt_client.setClientId(_client_id.c_str());

        // Mqtt connection events
        _mqtt_client.onConnect([this](bool sessionPresent) {
            debugPrint()->println("[MQTT] Connected to MQTT.");
            debugPrint()->print("[MQTT] Session present: ");
            debugPrint()->println(sessionPresent);

            for (auto it = _mlsm.begin(); it != _mlsm.end(); ++it) {
                uint16_t packetIdSub = _mqtt_client.subscribe(it->first.c_str(), 2);

                debugPrint()->print("[MQTT] Subscribing <");
                debugPrint()->print(it->first);
                debugPrint()->print("> at QoS 2, packetId: ");
                debugPrint()->println(packetIdSub);
            }
        });

        _mqtt_client.onDisconnect([this](AsyncMqttClientDisconnectReason reason) {
            debugPrint()->println("[MQTT] Disconnected from MQTT.");

            if (WiFi.isConnected()) {
                debugPrint()->println("[MQTT] Reconnect after 2 seconds...");
                _reconnect_ticker.once(2, [this]() {
                    debugPrint()->println("[MQTT] Reconnecting...");

                    _connect();
                });
            }
        });

        // Mqtt subscribe events
        _mqtt_client.onSubscribe([this](uint16_t packetId, uint8_t qos) {
            debugPrint()->println("[MQTT] Subscribe acknowledged:");
            debugPrint()->print  ("         packetId: "); debugPrint()->println(packetId);
            debugPrint()->print  ("              qos: "); debugPrint()->println(qos);
        });

        _mqtt_client.onUnsubscribe([this](uint16_t packetId) {
            debugPrint()->println("[MQTT] Unsubscribe acknowledged:");
            debugPrint()->print  ("         packetId: "); debugPrint()->println(packetId);
        });

        // Mqtt pub/msg events
        _mqtt_client.onPublish([this](uint16_t packetId) {
            debugPrint()->println("[MQTT] Publish acknowledged:");
            debugPrint()->print  ("         packetId: "); debugPrint()->println(packetId);
        });

        _mqtt_client.onMessage([this](char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
            auto msg = String("");
            for (size_t i = 0; i < len; i ++) {
                msg += payload[i];
            }

            debugPrint()->println("[MQTT] Message received:");
            debugPrint()->print  ("          topic: "); debugPrint()->println(topic);
            debugPrint()->print  ("            qos: "); debugPrint()->println(properties.qos);
            debugPrint()->print  ("            dup: "); debugPrint()->println(properties.dup);
            debugPrint()->print  ("         retain: "); debugPrint()->println(properties.retain);
            debugPrint()->print  ("            len: "); debugPrint()->println(len);
            debugPrint()->print  ("          index: "); debugPrint()->println(index);
            debugPrint()->print  ("          total: "); debugPrint()->println(total);
            debugPrint()->print  ("        payload: "); debugPrint()->println(msg);

            if (_mlsm.count(topic) == 1) {
                auto lst = _mlsm.at(topic);
                for (auto it = lst.begin(); it != lst.end(); ++it) {
                    (*it)(topic, msg, _mqtt_client);
                }
            }
        });

        // WiFi events
        _got_ip_handler = WiFi.onStationModeGotIP([this](const WiFiEventStationModeGotIP &) {
            debugPrint()->println("[MQTT] Connected to WiFi.");

            _connect();
        });

        _disconnected_handler = WiFi.onStationModeDisconnected([this](const WiFiEventStationModeDisconnected &) {
            debugPrint()->println("[MQTT] Disconnected from WiFi.");

            _reconnect_ticker.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
        });
    };

    void loop() {
    };

private:
    void _connect() {
        debugPrint()->print("[MQTT] Connecting to MQTT server: ");
        debugPrint()->print(_host);
        debugPrint()->print(":");
        debugPrint()->print(_port);
        debugPrint()->println();

        _mqtt_client.connect();
    };

private:
    String _host;
    int _port;
    String _client_id;

    typedef std::list<MessageListener> MessageListenerList;
    typedef std::map<String, MessageListenerList> MessageListenersMap;

    MessageListenersMap _mlsm;
    AsyncMqttClient _mqtt_client;

    WiFiEventHandler _got_ip_handler;
    WiFiEventHandler _disconnected_handler;

    Ticker _reconnect_ticker;
};
