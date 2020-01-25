#include <Arduino.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <simpleDSTadjust.h>
#include <timelib.h>
#include "ArduinoJson.h"

// #define DEV
#define VERSION 1.1

#ifdef DEV
#define THINGSPEAK_KEY "TRBAF4LV7U76SVST"
#else
#define THINGSPEAK_KEY "UUOUQ4MFC037UTYD"
#endif

#define WIFI_NAME "TP-LINK_GUEST_F092"
#define WIFI_PASS "1takovenormalnipripojeni2takovenormalnipripojeni3#"
// #define WIFI_NAME "Tieto Guest"
// #define WIFI_PASS "k9wh1sper"

#ifdef DEV
#define AP_WIFI_NAME "HOME_DEV"
#else
#define AP_WIFI_NAME "HOME"
#endif
#define AP_WIFI_PASS "12345678"

#define SENSOR_CO2_MAX 1000

#define TS_TEMP_OUT 0
#define TS_HUMI_OUT 1
#define TS_VENT 2
#define TS_CO2 3
#define TS_TEMP_IN 4
#define TS_HUMI_IN 5

#define VENT_RELAY_PIN 22

#define FILE_SIZE 3000

unsigned long logPointer = 0;

struct Weather {
    bool valid;
    int pressure;
    int humidity;
    float temp;
};

bool useSensor = true;
bool useVent = true;

String thingData[] = {"", "", "", "", "", "", "", ""};
String lastData[] = {"", "", "", "", "", "", "", ""};
time_t lastDataTime[8] = {};
bool thingDataValid;

time_t ventNextStart;
time_t ventNextStop;
bool ventRunning = false;
int ventConfig[] = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10};

#define NTP_SERVERS "us.pool.ntp.org", "pool.ntp.org", "time.nist.gov"
#define timezone +1
struct dstRule StartRule = {"CEST", Last, Sun, Mar, 2, 3600};
struct dstRule EndRule = {"CET", Last, Sun, Oct, 2, 0};
simpleDSTadjust dstAdjusted(StartRule, EndRule);

String logString = "";

AsyncWebServer server(80);

HTTPClient httpClient1;
HTTPClient httpClient2;

String toString(bool b) {
    return b ? "true" : "false";
}

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

void wifiEvent(WiFiEvent_t event) {
    log("Wifi event: " + String(event));
    switch (event) {
        case SYSTEM_EVENT_AP_START:
            log("IP address AP: ");
            log(WiFi.softAPIP().toString());
            break;
        case SYSTEM_EVENT_AP_STOP:
            break;
        case SYSTEM_EVENT_AP_STACONNECTED:
            break;
        case SYSTEM_EVENT_AP_STADISCONNECTED:
            break;
        case SYSTEM_EVENT_STA_START:
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            log("IP address: ");
            log(WiFi.localIP().toString());
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            break;
    }
}
void WIFI_Connect() {
    WiFi.disconnect();
    log("Connecting to WiFi...");
    WiFi.mode(WIFI_AP_STA);
    // WiFi.setHostname("test1");
    // WiFi.softAPsetHostname("test2");
    WiFi.softAP(AP_WIFI_NAME, AP_WIFI_PASS);
    WiFi.begin(WIFI_NAME, WIFI_PASS);
    WiFi.onEvent(wifiEvent);

    for (int i = 0; i < 60; i++) {
        if (WiFi.status() != WL_CONNECTED) {
            log(". ");
            delay(500);
        }
    }
    if (WiFi.status() == WL_CONNECTED) {
        log("WiFi Connected");
    }
}

void updateTime() {
    log("Updating time");
    configTime(timezone * 3600, 0, NTP_SERVERS);
    log("Waiting for time");
    while (time(nullptr) <= 1500000000) {
        log("# ");
        delay(500);
    }
    log("Waiting for adjust");
    while (!dstAdjusted.time(nullptr)) {
        log("# ");
        delay(500);
    }
    time_t t = dstAdjusted.time(NULL);
    setTime(hour(t) + 1, minute(t), second(t), day(t), month(t), year(t));
    log("to");
    log(formatTime(now()));
}

void updateTimeRequest(AsyncWebServerRequest *request) {
    updateTime();
    request->send(200);
}

void sendDataToThingspeak() {
    if (thingDataValid && WiFi.status() == WL_CONNECTED) {
        log("Thingspeak data sending");
        String url = "https://api.thingspeak.com/update?api_key=" THINGSPEAK_KEY;
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

void printTime(String text, time_t time) {
    log(text + " " + formatTime(time));
}

void setValue(int slot, String value) {
    thingDataValid = true;
    thingData[slot] = value;
    lastData[slot] = value;
    lastDataTime[slot] = now();
}

String getCorrectLastData(int channel) {
    if (lastData[channel] && lastDataTime[channel] + 300 > now()) {
        return lastData[channel];
    } else {
        return "";
    }
}

void countNextTimes(time_t tNow) {
    int h = hour(tNow);
    TimeElements te;
    breakTime(tNow, te);
    te.Second = 0;
    te.Minute = 0;
    do {
        h = h + 1;
        te.Hour = h;
        ventNextStart = makeTime(te);
        te.Minute = ventConfig[hour(ventNextStart)];
    } while (te.Minute == 0);
    ventNextStop = makeTime(te);
    printTime("Vent next start: ", ventNextStart);
    printTime("Vent next stop: ", ventNextStop);
}

void startVent() {
    String co2 = getCorrectLastData(TS_CO2);
    if (useSensor && co2 && co2.toInt() < 1000) {
        log("Vent auto disabled value=" + co2 + ", time=" + formatTime(lastDataTime[TS_CO2]));
        countNextTimes(now());
        return;
    }
    String hum = getCorrectLastData(TS_HUMI_IN);
    if (useSensor && hum && hum.toInt() < 38 && !ventRunning) {
        log("Vent auto disabled value=" + hum + ", time=" + formatTime(lastDataTime[TS_HUMI_IN]));
        countNextTimes(now());
        return;
    }
    log("Vent starting");
    ventRunning = true;
    digitalWrite(VENT_RELAY_PIN, LOW);
    setValue(TS_VENT, "1");
}

void stopVent() {
    log("Vent stopping");
    digitalWrite(VENT_RELAY_PIN, HIGH);
    ventRunning = false;
    setValue(TS_VENT, "0");
}

void countNextManTimes(int ventTime) {
    time_t tNow = now();
    TimeElements te;
    breakTime(tNow, te);
    te.Minute = te.Minute + ventTime;
    ventNextStart = tNow;
    ventNextStop = makeTime(te);
    printTime("Vent next m start: ", tNow);
    printTime("Vent next m stop: ", ventNextStop);
}

void handleVent() {
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
}

void handleManVent(int time) {
    countNextManTimes(time);
    if (ventNextStart != ventNextStop) {  // click vent 0
        startVent();
    }
}

void sensorRequest(AsyncWebServerRequest *request) {
    if (request->hasParam("CO2")) {
        AsyncWebParameter *namePar = request->getParam("CO2");
        String value = namePar->value().c_str();
        log("Sensor CO2: " + value);
        int intValue = value.toInt();
        setValue(TS_CO2, value);
        if (SENSOR_CO2_MAX < intValue && useSensor) {
            handleManVent(15);
        }
    }
    if (request->hasParam("HUM")) {
        AsyncWebParameter *namePar = request->getParam("HUM");
        String value = namePar->value().c_str();
        log("Sensor HUMIDITY: " + value);
        setValue(TS_HUMI_IN, value);
    }
    if (request->hasParam("TEMP")) {
        AsyncWebParameter *namePar = request->getParam("TEMP");
        String value = namePar->value().c_str();
        log("Sensor TEMPERATURE: " + value);
        setValue(TS_TEMP_IN, value);
    }
    request->send(200);
}

Weather loadWeather() {
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

void getWeather() {
    log("Getting weather");
    Weather weather = loadWeather();
    if (weather.valid) {
        log("Have weather");
        String temp = String(weather.temp);
        if (temp != lastData[TS_TEMP_OUT]) {
            setValue(TS_TEMP_OUT, temp);
        }
        String humidity = String(weather.humidity);
        if (humidity != lastData[TS_HUMI_OUT]) {
            setValue(TS_HUMI_OUT, humidity);
        }
    }
}

void handleApiLog(AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    response->print(logString);
    request->send(response);
    logString = "";
}

void handleApiPut(AsyncWebServerRequest *request) {
    if (request->hasParam("ventTime")) {
        AsyncWebParameter *vt = request->getParam("ventTime");
        String ventTimeStr = vt->value().c_str();
        handleManVent(ventTimeStr.toInt());
        return;
    }
    if (request->hasParam("sensor")) {
        AsyncWebParameter *vt = request->getParam("sensor");
        String sensor = vt->value().c_str();
        useSensor = sensor == "true";
        EEPROM.write(24, useSensor);
        log("Sensor: " + sensor);
    }
    if (request->hasParam("vent")) {
        AsyncWebParameter *vt = request->getParam("vent");
        String vent = vt->value().c_str();
        useVent = vent == "true";
        EEPROM.write(25, useVent);
        log("Ventilation: " + vent);
    }
    for (int i = 0; i < 24; i++) {
        if (request->hasParam(String(i))) {
            AsyncWebParameter *t = request->getParam(String(i));
            String vConfig = t->value().c_str();
            ventConfig[i] = vConfig.toInt();
            EEPROM.write(i, (byte)ventConfig[i]);
        }
    }
    EEPROM.commit();
    if (!useVent) {
        countNextTimes(now());
        stopVent();
    }
    request->send(200);
}

void handleApiGet(AsyncWebServerRequest *request) {
    time_t t = now();
    String json = "{\n";
    json += " \"data\":{\n";
    json += "  \"temp_out\":\"" + lastData[TS_TEMP_OUT] + "\",\n";
    json += "  \"hum_out\":\"" + lastData[TS_HUMI_OUT] + "\",\n";
    json += "  \"temp_in\":\"" + getCorrectLastData(TS_TEMP_IN) + "\",\n";
    json += "  \"hum_in\":\"" + getCorrectLastData(TS_HUMI_IN) + "\",\n";
    json += "  \"co2\":\"" + getCorrectLastData(TS_CO2) + "\",\n";
    json += "  \"nextStart\":\"" + formatTime(ventNextStart) + "\",\n";
    json += "  \"nextStop\":\"" + formatTime(ventNextStop) + "\"\n";
    json += " },\n";
    json += " \"action\":{\n";
    json += "  \"ventilation\":" + toString(ventRunning) + ",\n";
    json += "  \"vent\":" + toString(useVent) + ",\n";
    json += "  \"sensor\":" + toString(useSensor) + "\n";
    json += " },";
    json += " \"config\":{\n";
    json += "  \"version\": \"";
    json += VERSION;
    json += "\",\n";
    json += "  \"time\":{";
    json += "   \"year\":\"" + String(year(t)) + "\",\n";
    json += "   \"month\":\"" + addZero(month(t)) + "\",\n";
    json += "   \"day\":\"" + addZero(day(t)) + "\",\n";
    json += "   \"hour\":\"" + addZero(hour(t)) + "\",\n";
    json += "   \"minute\":\"" + addZero(minute(t)) + "\",\n";
    json += "   \"second\":\"" + addZero(second(t)) + "\"\n";
    json += "  },\n";
    json += "  \"ventilation\":{\n";
    json += "   \"0\":" + String(ventConfig[0]);
    json += ",  \"1\":" + String(ventConfig[1]);
    json += ",  \"2\":" + String(ventConfig[2]);
    json += ",  \"3\":" + String(ventConfig[3]);
    json += ",  \"4\":" + String(ventConfig[4]);
    json += ",  \"5\":" + String(ventConfig[5]);
    json += ",  \"6\":" + String(ventConfig[6]);
    json += ",  \"7\":" + String(ventConfig[7]);
    json += ",  \"8\":" + String(ventConfig[8]);
    json += ",  \"9\":" + String(ventConfig[9]);
    json += ",  \"10\":" + String(ventConfig[10]);
    json += ",  \"11\":" + String(ventConfig[11]);
    json += ",  \"12\":" + String(ventConfig[12]);
    json += ",  \"13\":" + String(ventConfig[13]);
    json += ",  \"14\":" + String(ventConfig[14]);
    json += ",  \"15\":" + String(ventConfig[15]);
    json += ",  \"16\":" + String(ventConfig[16]);
    json += ",  \"17\":" + String(ventConfig[17]);
    json += ",  \"18\":" + String(ventConfig[18]);
    json += ",  \"19\":" + String(ventConfig[19]);
    json += ",  \"20\":" + String(ventConfig[20]);
    json += ",  \"21\":" + String(ventConfig[21]);
    json += ",  \"22\":" + String(ventConfig[22]);
    json += ",  \"23\":" + String(ventConfig[23]);
    json += "  }\n";
    json += " }\n";
    json += "}\n";

    AsyncResponseStream *response = request->beginResponseStream("text/json");
    response->print(json);
    request->send(response);
}

int checkValue(int val) {
    if (val < 0 || val > 60) {
        return 10;
    } else {
        return val;
    }
}

void otaInit() {
    // Port defaults to 8266
    // ArduinoOTA.setPort(8266);

    // Hostname defaults to esp8266-[ChipID]
    // ArduinoOTA.setHostname("myesp8266");

    // No authentication by default
    // ArduinoOTA.setPassword((const char *)"123");
    ArduinoOTA.onStart([]() {
        log("OTA Start");
    });
    ArduinoOTA.onEnd([]() {
        log("OTA End");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        log("OTA Progress: " + String((progress / (total / 100))));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        log("OTA Error: " + String(error));
        if (error == OTA_AUTH_ERROR)
            log("OTA Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
            log("OTA Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
            log("OTA Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
            log("OTA Receive Failed");
        else if (error == OTA_END_ERROR)
            log("OTA End Failed");
    });
    ArduinoOTA.begin();
}

void setup() {
    Serial.begin(115200);

    pinMode(VENT_RELAY_PIN, OUTPUT);
    digitalWrite(VENT_RELAY_PIN, HIGH);

    SPIFFS.begin();

    log("SPIFFS total: " + String(SPIFFS.totalBytes()));
    log("SPIFFS used: " + String(SPIFFS.usedBytes()));

    WIFI_Connect();

    server.onNotFound([](AsyncWebServerRequest *request) { request->send(404); });
    server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
    server.on("/api/timereset", HTTP_PUT, updateTimeRequest);
    server.on("/api/sensor", HTTP_GET, sensorRequest);
    server.on("/api", HTTP_GET, handleApiGet);
    server.on("/api", HTTP_PUT, handleApiPut);
    server.on("/log", HTTP_GET, handleApiLog);
    server.begin();

    EEPROM.begin(27);
    for (int i = 0; i < 24; i++) {
        ventConfig[i] = checkValue(EEPROM.read(i));
    }
    useSensor = (boolean)EEPROM.read(24);
    useVent = (boolean)EEPROM.read(25);

    thingDataValid = false;
    updateTime();
    countNextTimes(now());

    otaInit();

    log("SETUP FINISHED");
}

void loop() {
    ArduinoOTA.handle();

    static unsigned long wifiPrev = 0;
    if (millis() - wifiPrev >= 2000) {
        if (WiFi.status() != WL_CONNECTED) {
            log("wifi disconnected");
            WIFI_Connect();
        }
        wifiPrev = millis();
    }

    static unsigned long timePrev = 0;
    if (millis() - timePrev >= 3600000) {
        updateTime();
        timePrev = millis();
    }

    static unsigned long weatherPrev = 0;
    if (millis() - weatherPrev >= 180000 || weatherPrev == 0) {
        getWeather();
        weatherPrev = millis();
    }

    static unsigned long ventPrev = 0;
    if (millis() - ventPrev >= 10000 && useVent) {
        handleVent();
        ventPrev = millis();
    }

    static unsigned long thingPrev = 0;
    if (millis() - thingPrev >= 30000) {
        sendDataToThingspeak();
        thingPrev = millis();
    }
    delay(1000);
}
