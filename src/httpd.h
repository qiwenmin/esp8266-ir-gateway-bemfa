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

            auto v = std::unique_ptr<DynamicJsonDocument>(nullptr);

            if (path == "status") {
                v = _apiStatusGet();
            } else {
                request->send(404, "text/plain", "Not Found");
                return;
            }

            if (v) {
                auto response = request->beginResponseStream("application/json");
                serializeJsonPretty(*v, *response);
                request->send(response);
            } else {
                request->send(500, "text/plain", "Internal Server Error");
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
            auto fpath = "/site/" + path;
            if (LittleFS.exists(fpath)) {
                request->send(LittleFS, fpath, mimetype(path), false);
            } else {
                request->send(404, "text/plain", "Not Found");
            }
        });

        // route - not found
        _server.onNotFound([](AsyncWebServerRequest *request) {
            request->send(404, "text/plain", "Not Found");
        });

        _server.begin();
    };
private:
    AsyncWebServer _server;

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

        (*v)["ESP"]["heapFree"] = heapFree;
        (*v)["ESP"]["heapMaxFreeBlockSize"] = heapMaxFreeBlockSize;
        (*v)["ESP"]["heapFragmentation"] = heapFragmentation;

        (*v)["ESP"]["cpuFreqMHz"] = ESP.getCpuFreqMHz();
        (*v)["ESP"]["cycleCount"] = ESP.getCycleCount();
        (*v)["ESP"]["chipId"] = ESP.getChipId();
        (*v)["ESP"]["vcc"] = ESP.getVcc();

        return v;
    }
};
