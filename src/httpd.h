#pragma once

#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <LittleFS.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>

#include "DebugLog.h"

#include "version.h"

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
        _server.on("/version", HTTP_GET, [](AsyncWebServerRequest *request) {
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

        // route - `/api/xxxxxx`
        auto handler = new AsyncCallbackJsonWebHandler("/api", [](AsyncWebServerRequest *request, JsonVariant &json) {
            auto path = request->url();
            DEBUG_LOG("[HTTPD] Json request: "); DEBUG_LOG_LN(path);
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
};
