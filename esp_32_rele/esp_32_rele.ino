#include <WiFi.h>
#include <HTTPClient.h>
#include "ArduinoJson.h"
#include <SPIFFS.h>
#include <TimeLib.h>
#include <simpleDSTadjust.h>
#include "AsyncJson.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#define FILESYSTEM SPIFFS

const char* wifi_name = "TP-LINK_F092"; // Your Wifi network name here
const char* wifi_pass = "1takovenormalnipripojeni2takovenormalnipripojeni3#";    // Your Wifi network password here

struct Weather {
  boolean valid;
  int pressure;
  int humidity;
  float temp;
};

String thingData[] = {"", "", "", "", "", "", "", ""};
String lastData[] = {"", "", "", "", "", "", "", ""};
boolean thingDataValid;

int vent_delay = 50 * 60; // in seconds
int vent_time = 10 * 60; // in seconds

int vent_relay_pin = 22;
boolean ventRunning = true;

int ventConfig[] = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10};
boolean vent = false;

AsyncWebServer server(80);

#define NTP_SERVERS "us.pool.ntp.org", "pool.ntp.org", "time.nist.gov"
#define timezone +1
struct dstRule StartRule = {"CEST", Last, Sun, Mar, 2, 3600}; // Central European Summer Time = UTC/GMT +2 hours
struct dstRule EndRule = {"CET", Last, Sun, Oct, 2, 0};       // Central European Time = UTC/GMT +1 hour
simpleDSTadjust dstAdjusted(StartRule, EndRule);

int boostPin = 18;
int boostOutPin = 19;
boolean boostPressed = false;

void IRAM_ATTR isr() {
  boostPressed = true;
}

void setup() {
  Serial.begin(115200);

  pinMode (vent_relay_pin, OUTPUT);

  pinMode (boostPin, INPUT_PULLUP);
  attachInterrupt(boostPin, isr, FALLING);

  thingDataValid = false;

  FILESYSTEM.begin();

  WiFi.begin(wifi_name, wifi_pass);
  Serial.println("Connecting to WiFi..");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println(".");
  }
  Serial.println("Connected to the WiFi network");
  Serial.println(WiFi.localIP());

  Serial.println("SETUP FINISHED");

  xTaskCreatePinnedToCore(loopWeather, "loopWeather", 8192, NULL, 3, NULL, 0);
  delay(500);

  xTaskCreatePinnedToCore(loopVent, "looVent", 8192, NULL, 1, NULL, 0);
  delay(500);

  xTaskCreatePinnedToCore(loopThingSpeakSend, "loopThingSpeakSend", 8192, NULL, 2, NULL, 0);
  delay(500);

  xTaskCreatePinnedToCore(loopUpdateTime, "loopUpdateTime", 8192, NULL, 2, NULL, 0);
  delay(500);

  server.onNotFound([](AsyncWebServerRequest * request) {
    request->send(404);
  });
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/favicon.ico", "image/x-icon");
  });
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/index.html", "text/html");
  });
  server.on("/api", HTTP_GET, handleApiGet);
  server.on("/api", HTTP_PUT, handleApiPut);
  server.begin();
}

void loop() {
  handleBoost();
  delay(1);
}

void loopUpdateTime(void *pvParameters) {
  while (true) {
    Serial.println("loopUpdateTime");
    configTime(timezone * 3600, 0, NTP_SERVERS);
    delay(500);
    while (!time(nullptr)) {
      Serial.print("#");
      delay(1000);
    }
    time_t t = dstAdjusted.time(NULL);
    setTime(hour(t) + 1, minute(t), second(t), day(t), month(t), year(t));
    delay(10000);
  }
}

void loopThingSpeakSend(void *pvParameters) {
  while (true) {
    sendDataToThingspeak();
    delay(15000);
  }
}

void loopVent(void *pvParameters) {
  time_t lastStart = now();
  while (true) {
    if (ventRunning) {
      Serial.println("Vent running");
      time_t t = now();
      int h = hour(t);
      int m = minute(t);
      int ventTime = ventConfig[h]; // time in minutes

      TimeElements te;
      breakTime(lastStart, te);
      te.Minute = te.Minute + 56;
      time_t nextStop = makeTime(te);
      Serial.println("nextStop: " + String(hour(nextStop)));
      Serial.println("nextStop: " + String(minute(nextStop)));
      Serial.println("nextStop: " + String(second(nextStop)));
      Serial.println("lastStart: " + String(hour(lastStart)));
      Serial.println("lastStart: " + String(minute(lastStart)));
      Serial.println("lastStart: " + String(second(lastStart)));
      ventRunning = false;
    } else {
      Serial.println("Vent not running");
      lastStart = now();
      ventRunning = true;
    }
    //    runVent(vent_time, vent_delay);
    delay(10000);
  }
}

void runVent(int vt, int vd) {
  startVent(vt);
  stopVent(vd);
}

void startVent(int vt) {
  Serial.println("Starting vent");
  vent = true;
  digitalWrite(vent_relay_pin, LOW);
  thingDataValid = true;
  thingData[3] = "1";
  delay(vt * 1000);
}

void stopVent(int vd) {
  Serial.println("Stopping vent");
  digitalWrite(vent_relay_pin, HIGH);
  vent = false;
  thingDataValid = true;
  thingData[3] = "0";
  delay(vd * 1000);
}

void loopWeather(void *pvParameters) {
  while (true) {
    Serial.println("Getting weather");
    Weather weather = getWeather();
    if (weather.valid) {
      String temp = String(weather.temp);
      if (temp != lastData[0]) {
        thingDataValid = true;
        thingData[0] = temp;
      }
      String humidity = String(weather.humidity);
      if (humidity != lastData[1]) {
        thingDataValid = true;
        thingData[1] = humidity;
      }
      String pressure = String(weather.pressure);
      if (pressure != lastData[2]) {
        thingDataValid = true;
        thingData[2] = pressure;
      }
    }
    delay(60000);
  }
}

void handleBoost() {
  if (boostPressed) {
    Serial.println("BOOST");
    delay(500);
    digitalWrite(boostOutPin, LOW);
    delay(500);
    digitalWrite(vent_relay_pin, HIGH);
    boostPressed = false;
  }
}

Weather getWeather() {
  HTTPClient httpClient;
  httpClient.begin("http://api.openweathermap.org/data/2.5/weather?id=3063471&appid=18d2229ced1ff3062d208b42f872abbc&units=metric");
  int httpCode = httpClient.GET();
  if (httpCode > 0) {
    //    Serial.printf("Weather code: %d\n", httpCode);
    if (httpCode == HTTP_CODE_OK) {
      String payload = httpClient.getString();
      //      Serial.println(payload);
      DynamicJsonDocument doc(2048);
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
      } else {
        float temp = doc["main"]["temp"];
        int pressure = doc["main"]["pressure"];
        int humidity = doc["main"]["humidity"];
        httpClient.end();
        payload = String();
        return {true, pressure, humidity, temp};
      }
    } else {
      Serial.println("Cannot get weather!");
    }
  }
  httpClient.end();
  return {false};
}

void sendDataToThingspeak() {
  if (thingDataValid) {
    HTTPClient httpClient;
    String url = "https://api.thingspeak.com/update?api_key=TRBAF4LV7U76SVST";
    url += thingData[0] != "" ? "&field1=" + thingData[0] : "";
    url += thingData[1] != "" ? "&field2=" + thingData[1] : "";
    url += thingData[2] != "" ? "&field3=" + thingData[2] : "";
    url += thingData[3] != "" ? "&field4=" + thingData[3] : "";
    url += thingData[4] != "" ? "&field5=" + thingData[4] : "";
    url += thingData[5] != "" ? "&field6=" + thingData[5] : "";
    url += thingData[6] != "" ? "&field7=" + thingData[6] : "";
    url += thingData[7] != "" ? "&field8=" + thingData[7] : "";
    Serial.println(url);
    httpClient.begin(url);
    int httpCode = httpClient.GET();
    if (httpCode > 0) {
      //    Serial.printf("Thingspeak code: %d\n", httpCode);
      if (httpCode == HTTP_CODE_OK) {
        //      Serial.println("Data sent to Thingspeak");
      } else {
        Serial.println("Cannot send data to Thingspeak!");
      }
    }
    httpClient.end();

    thingDataValid = false;
    for (int i = 0; i < 8; i++) {
      if (thingData[i] != "") {
        lastData[i] = thingData[i];
      }
      thingData[i] = "";
    }
  }
}

void handleApiPut(AsyncWebServerRequest *request) {
  if (request->hasParam("boost")) {
    AsyncWebParameter* b = request->getParam("boost");
    if (b->value().c_str() == "true") {
      boostPressed = true;
    }
  }
  if (request->hasParam("vent") && request->hasParam("ventTime")) {
    AsyncWebParameter* v = request->getParam("vent");
    AsyncWebParameter* vt = request->getParam("ventTime");
    Serial.println(v->value().c_str());
    Serial.println(vt->value().c_str());
  }
  for (int i = 0; i < 24; i++) {
    if (request->hasParam(String(i))) {
      AsyncWebParameter* t = request->getParam(String(i));
      Serial.println(t->value().c_str());
    }
  }
  request->send(200);
}

void handleApiGet(AsyncWebServerRequest *request) {
  time_t t = now();
  String json = "{";
  json += "\"data\":{";
  json += "\"temp\":" + lastData[0];
  json += ", \"humidity\":" + lastData[1];
  json += ", \"pressure\":" + lastData[2];
  json += "},";
  json += "  \"action\":{";
  json += "    \"boost\":" + String(boostPressed);
  json += ",    \"ventilation\":" + String(vent);
  json += "   },";
  json += "  \"config\":{";
  json += "\"time\":{";
  json += "\"year\":" + String(year(t)) + ",\n";
  json += "\"month\":" + String(month(t)) + ",\n";
  json += "\"day\":" + String(day(t)) + ",\n";
  json += "\"hour\":" + String(hour(t)) + ",\n";
  json += "\"minute\":" + String(minute(t)) + ",\n";
  json += "\"second\":" + String(second(t)) + "\n";
  json += "},";
  json += "\"ventilation\":{";
  json += "   \"0\":" + String(ventConfig[0]);
  json += ",   \"1\":" + String(ventConfig[1]);
  json += ",   \"2\":" + String(ventConfig[2]);
  json += ",   \"3\":" + String(ventConfig[3]);
  json += ",   \"4\":" + String(ventConfig[4]);
  json += ",   \"5\":" + String(ventConfig[5]);
  json += ",   \"6\":" + String(ventConfig[6]);
  json += ",   \"7\":" + String(ventConfig[7]);
  json += ",   \"8\":" + String(ventConfig[8]);
  json += ",   \"9\":" + String(ventConfig[9]);
  json += ",   \"10\":" + String(ventConfig[10]);
  json += ",   \"11\":" + String(ventConfig[11]);
  json += ",   \"12\":" + String(ventConfig[12]);
  json += ",   \"13\":" + String(ventConfig[13]);
  json += ",   \"14\":" + String(ventConfig[14]);
  json += ",   \"15\":" + String(ventConfig[15]);
  json += ",   \"16\":" + String(ventConfig[16]);
  json += ",   \"17\":" + String(ventConfig[17]);
  json += ",   \"18\":" + String(ventConfig[18]);
  json += ",   \"19\":" + String(ventConfig[19]);
  json += ",   \"20\":" + String(ventConfig[20]);
  json += ",   \"21\":" + String(ventConfig[21]);
  json += ",   \"22\":" + String(ventConfig[22]);
  json += ",   \"23\":" + String(ventConfig[23]);
  json += "    }";
  json += "  }";
  json += "}";

  AsyncResponseStream *response = request->beginResponseStream("text/json");
  response->print(json);
  request->send(response);
}
