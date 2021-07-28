#pragma once

#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <LittleFS.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <ESP8266mDNS.h>

#include "DebugLog.h"

#include "version.h"

extern BemfaMqtt bemfaMqtt;

static const char *mimetype(const String &filename) {
    auto fn = filename;
    fn.toLowerCase();

    if (fn.endsWith(".htm") || fn.endsWith(".html")) {
        return "text/html";
    }
    if (fn.endsWith(".css")) {
        return "text/css";
    }
    if (fn.endsWith(".js")) {
        return "application/javascript";
    }
    if (fn.endsWith(".json")) {
        return "application/json";
    }
    if (fn.endsWith(".png")) {
        return "image/png";
    }
    if (fn.endsWith(".jpg") || fn.endsWith(".jpeg")) {
        return "image/jpeg";
    }
    if (fn.endsWith(".gif")) {
        return "image/gif";
    }

    return "text/plain";
}

class Httpd {
public:
    Httpd(uint16_t port)
        : _server(port) {
    };

    void begin() {
        // Init FS
        LittleFS.begin();

        // Init web server

        // route - /version
        _server.on("^\\/version$", HTTP_GET, [](AsyncWebServerRequest *request) {
            auto response = request->beginResponseStream("application/json");
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
        _server.on("^\\/api\\/(.*)$", HTTP_GET, [this](AsyncWebServerRequest *request) {
            auto path = request->pathArg(0);

            DEBUG_LOG("[HTTP] GET /api/"); DEBUG_LOG_LN(path);

            if (path == "status") {
                _apiStatusGet(request);
            } else {
                request->send(404, "text/plain", "Not Found");
            }
        });

        // route - POST/PUT `/api/xxxxxx`
        auto handler = new AsyncCallbackJsonWebHandler("/api", [](AsyncWebServerRequest *request, JsonVariant &json) {
            auto path = request->url();
            DEBUG_LOG("[HTTP] Json request: "); DEBUG_LOG_LN(path);
            DEBUG_LOG("          method: "); DEBUG_LOG_LN(request->methodToString());

            auto jsonObj = json.as<JsonObject>();
            DEBUG_LOG_LN("            keys: ");
            for (auto it = jsonObj.begin(); it != jsonObj.end(); ++it) {
                DEBUG_LOG("                  "); DEBUG_LOG_LN(it->key().c_str());
            }

            // TODO implement apis

            request->send(200);
        });
        handler->setMethod(HTTP_POST | HTTP_PUT);
        handler->setMaxContentLength(1024);
        _server.addHandler(handler);

        // route - static contents
        _server.on("^\\/(.*)$", HTTP_GET, [](AsyncWebServerRequest *request) {
            auto path = request->pathArg(0);
            if (path == "") {
                path = "index.html";
            }

            DEBUG_LOG("[HTTP] static: "); DEBUG_LOG_LN(path);
            request->send(LittleFS, "/site/" + path, mimetype(path), false);
        });

        // route - not found
        _server.onNotFound([](AsyncWebServerRequest *request) {
            request->send(404, "text/plain", "Not found");
        });

        _server.begin();
    };
private:
    AsyncWebServer _server;

    void _apiStatusGet(AsyncWebServerRequest *request) {
        auto response = request->beginResponseStream("application/json");
        DynamicJsonDocument v(384);

        v["wifi"]["ssid"] = WiFi.SSID();
        v["wifi"]["isConnected"] = WiFi.isConnected();
        v["wifi"]["hostname"] = WiFi.getHostname();
        v["wifi"]["localIp"] = WiFi.localIP().toString();

        v["mdns"]["isRunning"] = MDNS.isRunning();

        v["mqtt"]["isConnected"] = bemfaMqtt.getMqttClient().connected();
        v["mqtt"]["clientId"] = bemfaMqtt.getMqttClient().getClientId();

        uint32_t free;
        uint16_t maxFreeBlockSize;
        uint8_t fragmentation;
        ESP.getHeapStats(&free, &maxFreeBlockSize, &fragmentation);

        v["heap"]["free"] = free;
        v["heap"]["maxFreeBlockSize"] = maxFreeBlockSize;
        v["heap"]["fragmentation"] = fragmentation;

        serializeJsonPretty(v, *response);
        request->send(response);
    }
};
