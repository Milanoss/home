#include <Arduino.h>
#include <EEPROM.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <Ticker.h>
#include <simpleDSTadjust.h>
#include <timelib.h>
#include "ArduinoJson.h"

String logString = "";
// #define BOOST_INPUT_PIN 18
// #define BOOST_OUTPUT_PIN 19
#define VENT_RELAY_PIN 22

#define FILESYSTEM SPIFFS

int interval = 100;
unsigned long previousMillis = 0;

const char *wifi_name = "TP-LINK_GUEST_F092";
const char *wifi_pass = "1takovenormalnipripojeni2takovenormalnipripojeni3#";
// const char *wifi_name = "Tieto Guest";
// const char *wifi_pass = "k9wh1sper";

struct Weather {
    bool valid;
    int pressure;
    int humidity;
    float temp;
};
Ticker weatherTicker;

String thingData[] = {"", "", "", "", "", "", "", ""};
String lastData[] = {"", "", "", "", "", "", "", ""};
bool thingDataValid;
Ticker thingSpeakTicker;

time_t ventNextStart;
time_t ventNextStop;
bool ventRunning = false;
int ventConfig[] = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10};

#define NTP_SERVERS "us.pool.ntp.org", "pool.ntp.org", "time.nist.gov"
#define timezone +1
struct dstRule StartRule = {"CEST", Last, Sun, Mar, 2, 3600};
struct dstRule EndRule = {"CET", Last, Sun, Oct, 2, 0};
simpleDSTadjust dstAdjusted(StartRule, EndRule);
bool timeOK = false;

// bool boostPressed = false;
// bool boostRunning = false;
// time_t boostNextStop;
// int boostTime = 1;
// portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

AsyncWebServer server(80);

HTTPClient httpClient1;
HTTPClient httpClient2;

// void IRAM_ATTR isr() {
//     portENTER_CRITICAL_ISR(&mux);
//     boostPressed = true;
//     portEXIT_CRITICAL_ISR(&mux);
// }

String addZero(int value) {
    if (value < 10) {
        return "0" + String(value);
    } else {
        return String(value);
    }
}

String formatTime(time_t time) {
    return addZero(hour(time)) + ":" + addZero(minute(time)) + ":" + addZero(second(time));
}

void log(String logMsg) {
    Serial.println(logMsg);
    if (logString.length() + logMsg.length() > 1000) {
        logString = "";
    } else {
        logString = logString + formatTime(now()) + "|" + logMsg + "\n";
    }
}
void WIFI_Connect() {
    WiFi.disconnect();
    log("Connecting to WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_name, wifi_pass);

    for (int i = 0; i < 60; i++) {
        if (WiFi.status() != WL_CONNECTED) {
            delay(250);
            // digitalWrite(ledPin, LOW);
            log(".");
            delay(250);
            // digitalWrite(ledPin, HIGH);
        }
    }
    if (WiFi.status() == WL_CONNECTED) {
        log("");
        log("WiFi Connected");
        log("IP address: ");
        log(WiFi.localIP());
    }
    // digitalWrite(ledPin, 0);
}

void loopUpdateTime(void *pvParameters) {
    while (true) {
        log("loopUpdateTime");
        configTime(timezone * 3600, 0, NTP_SERVERS);
        delay(500);
        while (!dstAdjusted.time(nullptr)) {
            log("#");
            delay(1000);
        }
        time_t t = dstAdjusted.time(NULL);
        setTime(hour(t) + 1, minute(t), second(t), day(t), month(t), year(t));
        timeOK = true;
        delay(86400000 * 3);  //one day
    }
}

void sendDataToThingspeak() {
    if (thingDataValid && WiFi.status() == WL_CONNECTED) {
        String url = "https://api.thingspeak.com/update?api_key=TRBAF4LV7U76SVST";
        url += thingData[0] != "" ? "&field1=" + thingData[0] : "";
        url += thingData[1] != "" ? "&field2=" + thingData[1] : "";
        url += thingData[2] != "" ? "&field3=" + thingData[2] : "";
        url += thingData[3] != "" ? "&field4=" + thingData[3] : "";
        url += thingData[4] != "" ? "&field5=" + thingData[4] : "";
        url += thingData[5] != "" ? "&field6=" + thingData[5] : "";
        url += thingData[6] != "" ? "&field7=" + thingData[6] : "";
        url += thingData[7] != "" ? "&field8=" + thingData[7] : "";
        log(url);
        httpClient2.begin(url);
        int httpCode = httpClient2.GET();
        log("Thingspeak code: " + String(httpCode));
        if (httpCode > 0) {
            String payload = httpClient2.getString();
            if (httpCode == HTTP_CODE_OK) {
                log("Data sent to Thingspeak: ");
                log(payload);
                if (!payload.startsWith("0")) {
                    thingDataValid = false;
                    for (int i = 0; i < 8; i++) {
                        if (thingData[i] != "") {
                            lastData[i] = thingData[i];
                        }
                        thingData[i] = "";
                    }
                } else {
                    log("Cannot send data to Thingspeak0!");
                }
            } else {
                log("Cannot send data to Thingspeak1!");
            }
        } else {
            log("Cannot send data to Thingspeak2!");
        }
        httpClient2.end();
    }
}

// void loopThingSpeakSend(void *pvParameters) {
void loopThingSpeakSend() {
    // delay(10000);
    // while (true) {
    log("loopThingSpeakSend");
    sendDataToThingspeak();
    //     delay(30000);
    // }
}

void printTime(String text, time_t time) {
    log(text + " " + formatTime(time));
}

void startVent() {
    log("Vent starting");
    ventRunning = true;
    digitalWrite(VENT_RELAY_PIN, LOW);
    thingDataValid = true;
    thingData[3] = "1";
}

void stopVent() {
    log("Vent stopping");
    digitalWrite(VENT_RELAY_PIN, HIGH);
    ventRunning = false;
    thingDataValid = true;
    thingData[3] = "0";
}

void countNextTimes(time_t tNow) {
    int h = hour(tNow) + 1;
    TimeElements te;
    breakTime(tNow, te);
    te.Second = 0;
    te.Minute = 0;
    te.Hour = h;
    ventNextStart = makeTime(te);
    te.Minute = ventConfig[hour(ventNextStart)];
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
            log("Vent running");
            if (ventNextStop < tNow) {
                stopVent();
                countNextTimes(tNow);
            }
        } else {
            log("Vent not running");
            if (ventNextStart < tNow) {
                startVent();
            }
        }
        delay(10000);
    }
}

Weather getWeather() {
    float temp = 0;
    int pressure = 0;
    int humidity = 0;
    bool result = false;
    if (WiFi.status() == WL_CONNECTED) {
        httpClient1.begin("http://api.openweathermap.org/data/2.5/weather?id=3063471&appid=18d2229ced1ff3062d208b42f872abbc&units=metric");
        int httpCode = httpClient1.GET();
        if (httpCode > 0) {
            log("Weather code: " + String(httpCode));
            if (httpCode == HTTP_CODE_OK) {
                // String payload = httpClient1.getString();
                // log(payload);
                DynamicJsonDocument doc(4096);
                DeserializationError error = deserializeJson(doc, httpClient1.getStream());
                if (error) {
                    log(F("deserializeJson() failed: "));
                    log(error.c_str());
                } else {
                    temp = doc["main"]["temp"];
                    pressure = doc["main"]["pressure"];
                    humidity = doc["main"]["humidity"];
                    result = true;
                }
            } else {
                log("Cannot get weather!");
            }
        }
        httpClient1.end();
    }
    return {result, pressure, humidity, temp};
}

// void loopWeather(void *pvParameters) {
void loopWeather() {
    // while (true) {
    log("loopWeather");
    Weather weather = getWeather();
    if (weather.valid) {
        log("Have weather");
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
    //     delay(300000);  // 5 minutes
    // }
}

// void handleBoost() {
// if (boostPressed) {
//     log("BOOST");

//     thingData[4] = "2";
//     thingDataValid = true;
//     sendDataToThingspeak();

//     delay(500);
//     digitalWrite(BOOST_OUTPUT_PIN, LOW);
//     delay(1000);
//     digitalWrite(BOOST_OUTPUT_PIN, HIGH);

//     delay(boostTime * 60 * 1000);
//     thingData[4] = "1";
//     thingDataValid = true;
//     sendDataToThingspeak();

//     portENTER_CRITICAL_ISR(&mux);
//     boostPressed = false;
//     portEXIT_CRITICAL_ISR(&mux);
// }
//     if (boostPressed) {
//         log("Boost start");
//         digitalWrite(BOOST_OUTPUT_PIN, LOW);
//         thingData[4] = "2";
//         thingDataValid = true;
//         boostPressed = false;
//         boostRunning = true;

//         time_t tNow = now();
//         TimeElements te;
//         breakTime(tNow, te);
//         te.Minute = te.Minute + boostTime;
//         boostNextStop = makeTime(te);

//         delay(500);
//         digitalWrite(BOOST_OUTPUT_PIN, HIGH);
//     }
//     if (boostRunning) {
//         log("Boost running");
//         if (boostNextStop < now()) {
//             thingData[4] = "1";
//             thingDataValid = true;
//             boostRunning = false;
//             log("Boost stop");
//         }
//     }
// }

void handleApiPut(AsyncWebServerRequest *request) {
    // if (request->hasParam("boost")) {
    //     AsyncWebParameter *b = request->getParam("boost");
    //     if (strcmp(b->value().c_str(), "true") == 0) {
    //         portENTER_CRITICAL_ISR(&mux);
    //         boostPressed = true;
    //         portEXIT_CRITICAL_ISR(&mux);
    //     }
    // }
    if (request->hasParam("ventTime")) {
        AsyncWebParameter *vt = request->getParam("ventTime");
        String ventTimeStr = vt->value().c_str();
        startVent();
        countNextManTimes(ventTimeStr.toInt());
    }
    // if (request->hasParam("boostTime")) {
    //     AsyncWebParameter *vt = request->getParam("boostTime");
    //     String boostTimeStr = vt->value().c_str();
    //     boostTime = boostTimeStr.toInt();
    //     EEPROM.write(24, (byte)boostTime);
    //     EEPROM.commit();
    // }
    for (int i = 0; i < 24; i++) {
        bool count = false;
        if (request->hasParam(String(i))) {
            AsyncWebParameter *t = request->getParam(String(i));
            String vConfig = t->value().c_str();
            ventConfig[i] = vConfig.toInt();
            EEPROM.write(i, (byte)ventConfig[i]);
            count = true;
        }
        if (count) {
            EEPROM.commit();
            stopVent();
            countNextTimes(now());
        }
    }
    request->send(200);
}

void handleApiGet(AsyncWebServerRequest *request) {
    time_t t = now();
    String json = "{";
    json += "\"data\":{";
    json += "\"temp\":\"" + lastData[0] + "\",\n";
    json += " \"humidity\":\"" + lastData[1] + "\",\n";
    json += " \"pressure\":\"" + lastData[2] + "\"\n";
    json += "},";
    json += "  \"action\":{";
    // json += "    \"boost\":" + String(boostRunning) + ",\n";
    json += "    \"ventilation\":" + String(ventRunning);
    json += "   },";
    json += "  \"config\":{";
    // json += "\"boost\":{";
    // json += "\"time\":" + String(boostTime) + "\n";
    // json += "},";
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

void handleApiLog(AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    response->print(logString);
    request->send(response);
    logString = "";
}

int checkValue(int val) {
    if (val < 0 || val > 60) {
        return 10;
    } else {
        return val;
    }
}

void setup() {
    Serial.begin(115200);

    pinMode(VENT_RELAY_PIN, OUTPUT);
    digitalWrite(VENT_RELAY_PIN, HIGH);
    // pinMode(BOOST_OUTPUT_PIN, OUTPUT);
    // digitalWrite(BOOST_OUTPUT_PIN, HIGH);

    // pinMode(BOOST_INPUT_PIN, INPUT);
    // attachInterrupt(digitalPinToInterrupt(BOOST_INPUT_PIN), isr, RISING);

    thingDataValid = false;

    FILESYSTEM.begin();

    // pinMode(ledPin, OUTPUT);
    WIFI_Connect();

    // xTaskCreatePinnedToCore(loopWeather, "loopWeather", 8192, NULL, 3, NULL, 1);
    // delay(500);

    xTaskCreatePinnedToCore(loopVent, "loopVent", 8192, NULL, 1, NULL, 1);
    delay(500);

    // xTaskCreatePinnedToCore(loopThingSpeakSend, "loopThingSpeakSend", 8192, NULL, 2, NULL, 1);
    // delay(500);

    xTaskCreatePinnedToCore(loopUpdateTime, "loopUpdateTime", 8192, NULL, 2, NULL, 1);
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
    server.on("/log.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/log.html", "text/html");
    });
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/style.css", "text/css");
    });
    server.on("/api", HTTP_GET, handleApiGet);
    server.on("/api", HTTP_PUT, handleApiPut);
    server.on("/log", HTTP_GET, handleApiLog);
    server.begin();

    httpClient1.setReuse(false);
    // httpClient2.setReuse(true);

    weatherTicker.attach(125, loopWeather);
    thingSpeakTicker.attach(30, loopThingSpeakSend);

    EEPROM.begin(25);
    // boostTime = checkValue(EEPROM.read(24));
    for (int i = 0; i < 24; i++) {
        ventConfig[i] = checkValue(EEPROM.read(i));
    }

    log("SETUP FINISHED");
}

void loop() {
    static unsigned long last = millis();
    if (millis() - last > 60000) {
        last = millis();
        log("Free heap: " + String(ESP.getFreeHeap()) + " bytes");
    }

    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval) {
        if (WiFi.status() != WL_CONNECTED) {
            log("wifi disconnected");
            WIFI_Connect();
        }
        // save the last time you blinked the LED
        previousMillis = currentMillis;
    }

    // handleBoost();
    // loopWeather();
    // loopThingSpeakSend();
    delay(10000);
}
