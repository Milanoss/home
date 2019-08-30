#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <TimeLib.h>
#include <simpleDSTadjust.h>

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

WebServer server(80);

#define NTP_SERVERS "us.pool.ntp.org", "pool.ntp.org", "time.nist.gov"
#define timezone +1
struct dstRule StartRule = {"CEST", Last, Sun, Mar, 2, 3600}; // Central European Summer Time = UTC/GMT +2 hours
struct dstRule EndRule = {"CET", Last, Sun, Oct, 2, 0};       // Central European Time = UTC/GMT +1 hour
simpleDSTadjust dstAdjusted(StartRule, EndRule);

void setup() {
  Serial.begin(115200);

  pinMode (vent_relay_pin, OUTPUT);

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

  server.onNotFound(handleNotFound);
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/data", HTTP_GET, handleApiGet);
  server.on("/api/config", HTTP_GET, handleApiGetConfig);
  server.begin();
}

void loop() {
  server.handleClient();
  delay(1);
}

void loopUpdateTime(void *pvParameters) {
  while (true) {
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
  while (true) {
    Serial.println("Starting vent");
    digitalWrite(vent_relay_pin, LOW);
    thingDataValid = true;
    thingData[3] = "1";
    delay(vent_time * 1000);

    Serial.println("Stopping vent");
    digitalWrite(vent_relay_pin, HIGH);
    thingDataValid = true;
    thingData[3] = "0";
    delay(vent_delay * 1000);
  }
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

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

bool exists(String path) {
  bool yes = false;
  File file = FILESYSTEM.open(path, "r");
  if (!file.isDirectory()) {
    yes = true;
  }
  file.close();
  return yes;
}

bool handleFileRead(String path) {
  if (path.endsWith("/")) {
    path += "index.html";
  }
  if (exists(path)) {
    File file = FILESYSTEM.open(path, "r");
    server.streamFile(file, "text/html");
    file.close();
    return true;
  } else {
    Serial.println("File not found");
  }
  return false;
}

void handleApiGetConfig() {
  time_t t = now();
  String json = "{";
  json += "\"time\":{";
  json += "\"year\":" + String(year(t)) + ",\n";
  json += "\"month\":" + String(month(t)) + ",\n";
  json += "\"day\":" + String(day(t)) + ",\n";
  json += "\"hour\":" + String(hour(t)) + ",\n";
  json += "\"minute\":" + String(minute(t)) + ",\n";
  json += "\"second\":" + String(second(t)) + "\n";
  json += "}";
  json += "}";
  server.send(200, "text/json", json);
  json = String();
}

void handleApiPutConfig() {
  time_t t = now();
  String json = "{";
  json += "\"time\":{";
  json += "\"year\":" + String(year(t)) + ",\n";
  json += "\"month\":" + String(month(t)) + ",\n";
  json += "\"day\":" + String(day(t)) + ",\n";
  json += "\"hour\":" + String(hour(t)) + ",\n";
  json += "\"minute\":" + String(minute(t)) + ",\n";
  json += "\"second\":" + String(second(t)) + "\n";
  json += "}";
  json += "}";
  server.send(200, "text/json", json);
  json = String();
}

void handleApiGet() {
  String json = "{";
  json += "\"temp\":" + lastData[0];
  json += ", \"humidity\":" + lastData[1];
  json += ", \"pressure\":" + lastData[2];
  json += "}";
  server.send(200, "text/json", json);
  json = String();
}
void handleRoot() {
  Serial.println("handleRoor");
  if (!handleFileRead("/index.html")) {
    server.send(404, "text/plain", "FileNotFound");
  }
}
