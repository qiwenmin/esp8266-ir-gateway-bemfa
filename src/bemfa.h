#pragma once

#include <Arduino.h>
#include <AsyncMqttClient.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <map>
#include <list>
#include <functional>
#include "DebugLog.h"

class BemfaMqtt {
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
            DEBUG_LOG_LN("[MQTT] Connected to MQTT.");
            DEBUG_LOG("[MQTT] Session present: ");
            DEBUG_LOG_LN(sessionPresent);

            for (auto it = _mlsm.begin(); it != _mlsm.end(); ++it) {
#ifdef ENABLE_DEBUG_LOG
                uint16_t packetIdSub =
#endif // ENABLE_DEBUG_LOG
                    _mqtt_client.subscribe(it->first.c_str(), 2);

                DEBUG_LOG("[MQTT] Subscribing <");
                DEBUG_LOG(it->first);
                DEBUG_LOG("> at QoS 2, packetId: ");
                DEBUG_LOG_LN(packetIdSub);
            }
        });

        _mqtt_client.onDisconnect([this](AsyncMqttClientDisconnectReason reason) {
            DEBUG_LOG_LN("[MQTT] Disconnected from MQTT.");

            if (WiFi.isConnected()) {
                DEBUG_LOG_LN("[MQTT] Reconnect after 2 seconds...");
                _reconnect_ticker.once(2, [this]() {
                    DEBUG_LOG_LN("[MQTT] Reconnecting...");

                    _connect();
                });
            }
        });

        // Mqtt subscribe events
        _mqtt_client.onSubscribe([this](uint16_t packetId, uint8_t qos) {
            DEBUG_LOG_LN("[MQTT] Subscribe acknowledged:");
            DEBUG_LOG   ("         packetId: "); DEBUG_LOG_LN(packetId);
            DEBUG_LOG   ("              qos: "); DEBUG_LOG_LN(qos);
        });

        _mqtt_client.onUnsubscribe([this](uint16_t packetId) {
            DEBUG_LOG_LN("[MQTT] Unsubscribe acknowledged:");
            DEBUG_LOG   ("         packetId: "); DEBUG_LOG_LN(packetId);
        });

        // Mqtt pub/msg events
        _mqtt_client.onPublish([this](uint16_t packetId) {
            DEBUG_LOG_LN("[MQTT] Publish acknowledged:");
            DEBUG_LOG   ("         packetId: "); DEBUG_LOG_LN(packetId);
        });

        _mqtt_client.onMessage([this](char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
            auto msg = String("");
            for (size_t i = 0; i < len; i ++) {
                msg += payload[i];
            }

            DEBUG_LOG_LN("[MQTT] Message received:");
            DEBUG_LOG   ("          topic: "); DEBUG_LOG_LN(topic);
            DEBUG_LOG   ("            qos: "); DEBUG_LOG_LN(properties.qos);
            DEBUG_LOG   ("            dup: "); DEBUG_LOG_LN(properties.dup);
            DEBUG_LOG   ("         retain: "); DEBUG_LOG_LN(properties.retain);
            DEBUG_LOG   ("            len: "); DEBUG_LOG_LN(len);
            DEBUG_LOG   ("          index: "); DEBUG_LOG_LN(index);
            DEBUG_LOG   ("          total: "); DEBUG_LOG_LN(total);
            DEBUG_LOG   ("        payload: "); DEBUG_LOG_LN(msg);

            if (_mlsm.count(topic) == 1) {
                auto lst = _mlsm.at(topic);
                for (auto it = lst.begin(); it != lst.end(); ++it) {
                    (*it)(topic, msg, _mqtt_client);
                }
            }
        });

        // WiFi events
        _got_ip_handler = WiFi.onStationModeGotIP([this](const WiFiEventStationModeGotIP &) {
            DEBUG_LOG_LN("[MQTT] Connected to WiFi.");

            _connect();
        });

        _disconnected_handler = WiFi.onStationModeDisconnected([this](const WiFiEventStationModeDisconnected &) {
            DEBUG_LOG_LN("[MQTT] Disconnected from WiFi.");

            _reconnect_ticker.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
        });
    };

    void loop() {
    };

    AsyncMqttClient &getMqttClient() {
        return _mqtt_client;
    };
private:
    void _connect() {
        DEBUG_LOG("[MQTT] Connecting to MQTT server: ");
        DEBUG_LOG(_host);
        DEBUG_LOG(":");
        DEBUG_LOG(_port);
        DEBUG_LOG_LN();

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
