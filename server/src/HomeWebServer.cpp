#include <HomeWebServer.h>

HomeWebServer::HomeWebServer() {
    server.onNotFound([](AsyncWebServerRequest *request) { request->send(404); });
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/favicon.ico", "image/x-icon");
    });
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/index.html", "text/html");
    });
    server.on("/config.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/config.html", "text/html");
    });
    server.on("/log.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/log.html", "text/html");
    });
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/style.css", "text/css");
    });
    server.begin();
}

AsyncCallbackWebHandler& HomeWebServer::on(const char* uri, WebRequestMethodComposite method, ArRequestHandlerFunction onRequest) {
    return server.on(uri, method, onRequest);
}