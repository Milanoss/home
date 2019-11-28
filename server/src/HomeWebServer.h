#ifndef HOME_WEB_SERVER_H
#define HOME_WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

class HomeWebServer {
   private:
    AsyncWebServer server = AsyncWebServer(80);

   public:
    HomeWebServer();
    AsyncCallbackWebHandler& on(const char* uri, WebRequestMethodComposite method, ArRequestHandlerFunction onRequest);
};

#endif