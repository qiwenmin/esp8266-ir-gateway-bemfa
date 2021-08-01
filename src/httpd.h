#pragma once

#include <memory>

#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <LittleFS.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <ESP8266mDNS.h>

#include "DebugLog.h"

#include "version.h"

#ifdef DEV_BOARD
#define ENABLE_CORS
#endif // DEV_BOARD

extern BemfaMqtt bemfaMqtt;

static constexpr size_t body_content_max_length = 1024;

class Httpd {
public:
    Httpd(uint16_t port)
        : _server(port) {
    };

    void begin(const String &realm = "ir-gateway") {
        _realm = realm;

        // Init FS
        LittleFS.begin();

        // Init web server

        // route - /version
        _server.on("/api/version", HTTP_GET, [this](AsyncWebServerRequest *request) {
            auto response = request->beginResponseStream("application/json");
#ifdef ENABLE_CORS
            _setCorsHeaders(request, response);
#endif // ENABLE_CORS

            DynamicJsonDocument v(256);

            v["firmware"] = FIRMWARE_VERSION;
            v["sdk"] = ESP.getSdkVersion();
            v["boot"] = ESP.getBootVersion();
            v["core"] = ESP.getCoreVersion();
            v["full"] = ESP.getFullVersion();

            serializeJsonPretty(v, *response);
            request->send(response);
        });

        // route - GET `/api/xxxxxx`
        _server.on("/api/*", HTTP_GET, [this](AsyncWebServerRequest *request) {
            if (!_authenticate(request)) return;

            auto path = request->url();

            DEBUG_LOG("[HTTP] GET "); DEBUG_LOG_LN(path);

            auto v = std::unique_ptr<DynamicJsonDocument>(nullptr);

            if (path == "/api/status") {
                v = _apiStatusGet();
            } else if (path == "/api/bemfa/config") {
                v = _apiBemfaConfigGet();
            } else if (path == "/api/bemfa/topics") {
                v = _apiBemfaTopicsGet();
            } else {
                request->send(404, "text/plain", "Not Found");
                return;
            }

            if (v) {
                auto response = request->beginResponseStream("application/json");
#ifdef ENABLE_CORS
                _setCorsHeaders(request, response);
#endif // ENABLE_CORS
                serializeJsonPretty(*v, *response);
                request->send(response);
            } else {
                request->send(500, "text/plain", "Internal Server Error");
            }
        });

        // route - POST/PUT `/api/xxxxxx`
        _server.on("/api/*", HTTP_POST | HTTP_PUT, [this](AsyncWebServerRequest *request) {
            if (!_authenticate(request)) return;

            auto ct = request->contentType();
            ct.toLowerCase();
            if (ct != "application/json" && !ct.startsWith("application/json;")) {
                return request->send(415, "text/plain", "Unsupported Media Type");
            }

            if (_body_content_length > body_content_max_length) {
                return request->send(413, "text/plain", "Payload Too Large");
            }
            if (_body_content_length == 0) {
                return request->send(400, "text/plain", "Bad Request");
            }
            if (request->_tempObject == nullptr) {
                return request->send(500, "text/plain", "Internal Server Error");
            }

            DynamicJsonDocument jsonBuffer(body_content_max_length);
            DeserializationError error = deserializeJson(jsonBuffer, (uint8_t*)(request->_tempObject));

            if (error) {
                return request->send(400, "text/plain", "Bad Request");
            }

            auto path = request->url();
            DEBUG_LOG("[HTTP] Json request: "); DEBUG_LOG_LN(path);
            DEBUG_LOG("          method: "); DEBUG_LOG_LN(request->methodToString());

            JsonVariant json = jsonBuffer.as<JsonVariant>();
            auto jsonObj = json.as<JsonObject>();

            DEBUG_LOG_LN("            keys: ");
            for (auto it = jsonObj.begin(); it != jsonObj.end(); ++it) {
                DEBUG_LOG("                  "); DEBUG_LOG_LN(it->key().c_str());
            }

            // TODO implement apis
            auto response = request->beginResponse(
                200, "application/json", R"({"rebootRequired":true})");

#ifdef ENABLE_CORS
            _setCorsHeaders(request, response);
#endif // ENABLE_CORS

            request->send(response);
        }, nullptr, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            _body_content_length = total;
            if (total > 0 && request->_tempObject == NULL && total < body_content_max_length) {
                request->_tempObject = malloc(total);
            }
            if (request->_tempObject != NULL) {
                memcpy((uint8_t*)(request->_tempObject) + index, data, len);
            }
        });

        // route - static contents
        _server.serveStatic("/", LittleFS, "/site/").setDefaultFile("index.html");

        // route - not found
        _server.onNotFound([this](AsyncWebServerRequest *request) {
#ifdef ENABLE_CORS
            if (request->method() == HTTP_OPTIONS) {
                auto response = request->beginResponse(200);
                _setCorsHeaders(request, response);
                return request->send(response);
            }
#endif // ENABLE_CORS
            request->send(404, "text/plain", "Not Found");
        });

        _server.begin();
    };
private:
    AsyncWebServer _server;
    String _realm;
    size_t _body_content_length;

    std::unique_ptr<DynamicJsonDocument> _apiStatusGet() {
        auto v = std::make_unique<DynamicJsonDocument>(512);

        (*v)["wifi"]["ssid"] = WiFi.SSID();
        (*v)["wifi"]["isConnected"] = WiFi.isConnected();
        (*v)["wifi"]["hostname"] = WiFi.getHostname();
        (*v)["wifi"]["localIp"] = WiFi.localIP().toString();
        (*v)["wifi"]["mac"] = WiFi.macAddress();

        (*v)["mdns"]["isRunning"] = MDNS.isRunning();

        (*v)["mqtt"]["isConnected"] = bemfaMqtt.getMqttClient().connected();
        (*v)["mqtt"]["clientId"] = bemfaMqtt.getMqttClient().getClientId();

        uint32_t heapFree;
        uint16_t heapMaxFreeBlockSize;
        uint8_t heapFragmentation;
        ESP.getHeapStats(&heapFree, &heapMaxFreeBlockSize, &heapFragmentation);

        (*v)["device"]["heapFree"] = heapFree;
        (*v)["device"]["heapMaxFreeBlockSize"] = heapMaxFreeBlockSize;
        (*v)["device"]["heapFragmentation"] = heapFragmentation;

        (*v)["device"]["cpuFreqMHz"] = ESP.getCpuFreqMHz();
        (*v)["device"]["cycleCount"] = ESP.getCycleCount();
        (*v)["device"]["chipId"] = ESP.getChipId();
        (*v)["device"]["vcc"] = ESP.getVcc();

        (*v)["device"]["uptimeInMs"] = millis();

        return v;
    };

    std::unique_ptr<DynamicJsonDocument> _apiBemfaConfigGet() {
        auto v = std::make_unique<DynamicJsonDocument>(256);

        (*v)["server"] = "bemfa.com";
        (*v)["port"] = 9501;
        (*v)["clientId"] = "cb99bd1269d5d25609795b817e940b0a";

        return v;
    };

    std::unique_ptr<DynamicJsonDocument> _apiBemfaTopicsGet() {
        auto v = std::make_unique<DynamicJsonDocument>(256);

        std::list<String> topics;
        topics = bemfaMqtt.getTopics(topics);

        for (auto it = topics.begin(); it != topics.end(); ++it) {
            // Here Chinese value is used because the MIOT platform uses Chinese name for voice commands.
            if (it->endsWith("002")) {
                (*v)[*it]["type"] = "Light";
                (*v)[*it]["name"] = "Panasonic Light 01";
                (*v)[*it]["defaultAlias"] = "灯";
            } else if (it->endsWith("005")) {
                (*v)[*it]["type"] = "AC";
                (*v)[*it]["name"] = "Electrolux AC 01";
                (*v)[*it]["defaultAlias"] = "空调";
            } else {
                (*v)[*it]["type"] = "Unknown";
                (*v)[*it]["name"] = "Unknown Device";
                (*v)[*it]["defaultAlias"] = "未知设备";
            }
        }

        return v;
    };

    bool _authenticate(AsyncWebServerRequest *request) {
        if (!request->authenticate("admin", "12345", _realm.c_str())) {
            //request->requestAuthentication(_realm.c_str(), true);
            auto response = request->beginResponse(401, "text/plain", "Unauthorized");
#ifdef ENABLE_CORS
            _setCorsHeaders(request, response);
#endif // ENABLE_CORS
            request->send(response);

            return false;
        }
        return true;
    };

#ifdef ENABLE_CORS
    void _setCorsHeaders(AsyncWebServerRequest *request, AsyncWebServerResponse *response) {
        if (request->method() == HTTP_GET) {
            response->addHeader("Access-Control-Allow-Origin", "*");
        } else {
            auto origin = request->getHeader("origin");
            response->addHeader("Access-Control-Allow-Origin", origin ? origin->value() : "*");
            response->addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS, PUT, DELETE");
            response->addHeader("Access-Control-Allow-Headers", "Origin,Content-Type,Accept,Authorization,X-Requested-With");
            response->addHeader("Access-Control-Allow-Credentials", "true");
        }
    }
#endif // ENABLE_CORS

};
