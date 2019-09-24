#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <simpleDSTadjust.h>
#include <timelib.h>
#include "ArduinoJson.h"

#define BOOST_INPUT_PIN 18
#define BOOST_OUTPUT_PIN 19
#define VENT_RELAY_PIN 22

#define FILESYSTEM SPIFFS

const char *wifi_name = "TP-LINK_F092";
const char *wifi_pass = "1takovenormalnipripojeni2takovenormalnipripojeni3#";
// const char *wifi_name = "Tieto Guest";
// const char *wifi_pass = "k9wh1sper";

struct Weather {
    boolean valid;
    int pressure;
    int humidity;
    float temp;
};

String thingData[] = {"", "", "", "", "", "", "", ""};
String lastData[] = {"", "", "", "", "", "", "", ""};
boolean thingDataValid;

time_t ventNextStart;
time_t ventNextStop;
boolean ventRunning = false;
int ventConfig[] = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10};

#define NTP_SERVERS "us.pool.ntp.org", "pool.ntp.org", "time.nist.gov"
#define timezone +1
struct dstRule StartRule = {"CEST", Last, Sun, Mar, 2, 3600};
struct dstRule EndRule = {"CET", Last, Sun, Oct, 2, 0};
simpleDSTadjust dstAdjusted(StartRule, EndRule);
boolean timeOK = false;

boolean boostPressed = false;
int boostTime = 1;

AsyncWebServer server(80);

void IRAM_ATTR isr() {
    boostPressed = true;
}

void loopUpdateTime(void *pvParameters) {
    while (true) {
        Serial.println("loopUpdateTime");
        configTime(timezone * 3600, 0, NTP_SERVERS);
        delay(500);
        while (!dstAdjusted.time(nullptr)) {
            Serial.print("#");
            delay(1000);
        }
        time_t t = dstAdjusted.time(NULL);
        setTime(hour(t) + 1, minute(t), second(t), day(t), month(t), year(t));
        timeOK = true;
        delay(3600000);
    }
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
               Serial.printf("Thingspeak code: %d\n", httpCode);
            if (httpCode == HTTP_CODE_OK) {
                     Serial.println("Data sent to Thingspeak");
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

void loopThingSpeakSend(void *pvParameters) {
    while (true) {
        sendDataToThingspeak();
        delay(15000);
    }
}

void printTime(String text, time_t time) {
    Serial.print(text);
    Serial.println(" " + String(hour(time)) + ":" + String(minute(time)) + ":" +
                   String(second(time)));
}

void startVent() {
    Serial.println("Vent starting");
    ventRunning = true;
    digitalWrite(VENT_RELAY_PIN, LOW);
    thingDataValid = true;
    thingData[3] = "1";
}

void stopVent() {
    Serial.println("Vent stopping");
    digitalWrite(VENT_RELAY_PIN, HIGH);
    ventRunning = false;
    thingDataValid = true;
    thingData[3] = "0";
}

void countNextTimes(time_t tNow) {
    int h = hour(tNow);
    int ventTime = ventConfig[h];  // time in minutes
    TimeElements te;
    breakTime(tNow, te);
    te.Second = 0;
    te.Minute = 0;
    te.Hour = te.Hour + 1;
    ventNextStart = makeTime(te);
    te.Minute = ventTime;
    ventNextStop = makeTime(te);
    printTime("Vent next start: ", ventNextStart);
    printTime("Vent next stop: ", ventNextStop);
}

void countNextManTimes(int ventTime) {
    time_t tNow = now();
    TimeElements te;
    breakTime(tNow, te);
    te.Minute = te.Minute + ventTime;
    ventNextStop = makeTime(te);
    printTime("Vent next m start: ", tNow);
    printTime("Vent next m stop: ", ventNextStop);
}

void loopVent(void *pvParameters) {
    while (!timeOK) {
        delay(5000);
    }
    countNextTimes(now());
    while (true) {
        time_t tNow = now();
        if (ventRunning) {
            Serial.println("Vent running");
            if (ventNextStop < tNow) {
                stopVent();
                countNextTimes(tNow);
            }
        } else {
            Serial.println("Vent not running");
            if (ventNextStart < tNow) {
                startVent();
            }
        }
        delay(5000);
    }
}

Weather getWeather() {
    HTTPClient httpClient;
    httpClient.begin("http://api.openweathermap.org/data/2.5/weather?id=3063471&appid=18d2229ced1ff3062d208b42f872abbc&units=metric");
    int httpCode = httpClient.GET();
    if (httpCode > 0) {
           Serial.printf("Weather code: %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK) {
            String payload = httpClient.getString();
                 Serial.println(payload);
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

void loopWeather(void *pvParameters) {
    while (true) {
        Serial.println("Getting weather");
        Weather weather = getWeather();
        if (weather.valid) {
            Serial.println("Have weather");
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
        delay(300000);// 5 minutes
    }
}

void handleBoost() {
    if (boostPressed) {
        Serial.println("BOOST");

        thingData[4] = "1";
        thingDataValid = true;
        sendDataToThingspeak();

        delay(500);
        digitalWrite(BOOST_OUTPUT_PIN, LOW);
        delay(1000);
        digitalWrite(BOOST_OUTPUT_PIN, HIGH);

        delay(boostTime * 60 * 1000);
        thingData[4] = "0";
        thingDataValid = true;
        sendDataToThingspeak();

        boostPressed = false;
    }
}

void handleApiPut(AsyncWebServerRequest *request) {
    if (request->hasParam("boost")) {
        AsyncWebParameter *b = request->getParam("boost");
        if (strcmp(b->value().c_str(), "true") == 0) {
            boostPressed = true;
        }
    }
    if (request->hasParam("ventTime")) {
        AsyncWebParameter *vt = request->getParam("ventTime");
        String ventTimeStr = vt->value().c_str();
        startVent();
        countNextManTimes(ventTimeStr.toInt());
    }
    if (request->hasParam("boostTime")) {
        AsyncWebParameter *vt = request->getParam("boostTime");
        String boostTimeStr = vt->value().c_str();
        boostTime = boostTimeStr.toInt();
    }
    for (int i = 0; i < 24; i++) {
        boolean count = false;
        if (request->hasParam(String(i))) {
            AsyncWebParameter *t = request->getParam(String(i));
            String vConfig = t->value().c_str();
            ventConfig[i] = vConfig.toInt();
        }
        if (count) {
            stopVent();
            countNextTimes(now());
        }
    }
    request->send(200);
}

String addZero(int value) {
    if (value < 10) {
        return "0" + String(value);
    } else {
        return String(value);
    }
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
    json += ",    \"ventilation\":" + String(ventRunning);
    json += "   },";
    json += "  \"config\":{";
    json += "\"boost\":{";
    json += "\"time\":" + String(boostTime) + "\n";
    json += "},";
    json += "\"time\":{";
    json += "\"year\":\"" + String(year(t)) + "\",\n";
    json += "\"month\":\"" + addZero(month(t)) + "\",\n";
    json += "\"day\":\"" + addZero(day(t)) + "\",\n";
    json += "\"hour\":\"" + addZero(hour(t)) + "\",\n";
    json += "\"minute\":\"" + addZero(minute(t)) + "\",\n";
    json += "\"second\":\"" + addZero(second(t)) + "\"\n";
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

void setup() {
    Serial.begin(115200);

    pinMode(VENT_RELAY_PIN, OUTPUT);
    digitalWrite(VENT_RELAY_PIN, HIGH);
    pinMode(BOOST_OUTPUT_PIN, OUTPUT);
    digitalWrite(BOOST_OUTPUT_PIN, HIGH);

    pinMode(BOOST_INPUT_PIN, INPUT_PULLUP);
    attachInterrupt(BOOST_INPUT_PIN, isr, FALLING);

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

    xTaskCreatePinnedToCore(loopVent, "loopVent", 8192, NULL, 1, NULL, 0);
    delay(500);

    xTaskCreatePinnedToCore(loopThingSpeakSend, "loopThingSpeakSend", 8192, NULL, 2, NULL, 0);
    delay(500);

    xTaskCreatePinnedToCore(loopUpdateTime, "loopUpdateTime", 8192, NULL, 2, NULL, 0);
    delay(500);

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
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/style.css", "text/css");
    });
    server.on("/api", HTTP_GET, handleApiGet);
    server.on("/api", HTTP_PUT, handleApiPut);
    server.begin();
}

void loop() {
    handleBoost();
    delay(1);
}
