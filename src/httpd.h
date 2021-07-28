#pragma once

#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <LittleFS.h>

class Httpd {
public:
    Httpd(uint16_t port)
        : _server(port) {
    };

    void begin() {
        // Init FS
        LittleFS.begin();

        // Init web server
        _server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(LittleFS, "/site/index.html", "text/html", false);
        });

        _server.onNotFound([](AsyncWebServerRequest *request) {
            request->send(404, "text/plain", "Not found");
        });

        _server.begin();
    };
private:
    AsyncWebServer _server;
};
