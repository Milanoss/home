#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>

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

int vent_delay = 30;
int vent_time = 20;

int vent_relay_pin = 22;

WebServer server(80);

void setup() {
  Serial.begin(115200);

  pinMode (vent_relay_pin, OUTPUT);

  thingDataValid = false;

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

  server.onNotFound(handleNotFound);
  server.on("/", handleRoot);
  server.begin();
}

void loop() {
  server.handleClient();
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
      thingDataValid = true;
      thingData[0] = String(weather.temp);
      thingData[1] = String(weather.humidity);
      thingData[2] = String(weather.pressure);
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
      StaticJsonDocument<790> doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
      } else {
        float temp = doc["main"]["temp"];
        int pressure = doc["main"]["pressure"];
        int humidity = doc["main"]["humidity"];
        httpClient.end();
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
    url += "&field1=" + thingData[0];
    url += "&field2=" + thingData[1];
    url += "&field3=" + thingData[2];
    url += "&field4=" + thingData[3];
    url += "&field5=" + thingData[4];
    url += "&field6=" + thingData[5];
    url += "&field7=" + thingData[6];
    url += "&field8=" + thingData[7];
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

void handleRoot() {

  char temp[700];

  snprintf(temp, 700,           "aa %s bb %s cc %s"           , lastData[0], lastData[1], lastData[2]);
  server.send(200, "text/html", lastData[0]);
}
