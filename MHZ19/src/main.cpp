#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include "MHZ19.h"
#include "RunningMedian.h"

// #define RX_PIN 10      // Rx pin which the MHZ19 Tx pin is attached to
// #define TX_PIN 11      // Tx pin which the MHZ19 Rx pin is attached to
#define RX_PIN 4       // Rx pin which the MHZ19 Tx pin is attached to
#define TX_PIN 5       // Tx pin which the MHZ19 Rx pin is attached to
#define BAUDRATE 9600  // Native to the sensor (do not change)

#define CLK 2
#define DIO 3
const String wifi_name = "HOME";
const String wifi_pass = "12345678";

int interval = 100;
unsigned long previousMillis = 0;

MHZ19 myMHZ19;                            // Constructor for MH-Z19 class
SoftwareSerial mySerial(RX_PIN, TX_PIN);  // Uno example

unsigned long getDataTimer = 0;  // Variable to store timer interval
unsigned long sendTimer = 0;
int CO2;
float Temp;

RunningMedian co2samples = RunningMedian(7);
RunningMedian tempSamples = RunningMedian(7);

void WIFI_Connect() {
    WiFi.disconnect();
    Serial.println("Connecting to WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_name, wifi_pass);

    for (int i = 0; i < 60; i++) {
        if (WiFi.status() != WL_CONNECTED) {
            delay(250);
            Serial.print(".");
            delay(250);
        }
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("");
        Serial.println("WiFi Connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
    }
}

void sendData() {
    IPAddress ip = WiFi.gatewayIP();
    WiFiClient client;
    HTTPClient httpClient1;

    float medianCo2 = co2samples.getMedian();
    if (isnan(medianCo2)) {
        return;
    }
    String value = String((int)medianCo2);
    String url = "http://" + ip.toString() + "/api/sensor?name=CO2&value=" + value;
    Serial.println(url);
    if (httpClient1.begin(client, url)) {
            int httpCode = httpClient1.GET();
            Serial.printf("Sending code: %d\n", httpCode);
            String payload = httpClient1.getString();
            if (httpCode == HTTP_CODE_OK) {
                Serial.print("Data sent");
            } else {
                Serial.println("Cannot send data!");
            }
            httpClient1.end();
        } else {
            Serial.println("[HTTP} Unable to connect");
        }
}

void setup() {
    Serial.begin(9600);  // For ESP32 baudarte is 115200 etc.

    mySerial.begin(BAUDRATE);

    //mySerial.begin(BAUDRATE, SERIAL_8N1, RX_PIN, TX_PIN); // ESP32 Example

    myMHZ19.begin(mySerial);

    myMHZ19.autoCalibration(false);

    WIFI_Connect();
}

void loop() {
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("wifi disconnected ");
            WIFI_Connect();
        }
        previousMillis = currentMillis;
    }

    // int val = 0;
    // val = digitalRead(7);  // read input value
    // if (val == LOW) {     // check if the input is HIGH (button released)
    //     Serial.println("CALIBRATION");
    //     myMHZ19.calibrateZero();
    // }
    if (millis() - sendTimer >= 120000) {
        sendData();
        sendTimer = millis();
    }
    if (millis() - getDataTimer >= 20000)
    {
        /* note: getCO2() default is command "CO2 Unlimited". This returns the correct CO2 reading even 
        if below background CO2 levels or above range (useful to validate sensor). You can use the 
        usual documented command with getCO2(false) */

        CO2 = myMHZ19.getCO2();
        Temp = myMHZ19.getTemperature(true, true);
        if (myMHZ19.errorCode == RESULT_OK && CO2 > 0 && CO2 < 5000) {
            co2samples.add(CO2);
            Serial.print("CO2 (ppm): ");
            Serial.println(CO2);
            tempSamples.add(Temp);
            Serial.print("Temperature (C): ");
            Serial.println(Temp);
        }
        // Serial.println("getCO2(false)" + String(myMHZ19.getCO2(false)));
        // Serial.println("getCO2(true)" + String(myMHZ19.getCO2(true)));
        // Serial.println("getCO2Raw(false)" + String(myMHZ19.getCO2Raw(false)));
        // Serial.println("getCO2Raw(true)" + String(myMHZ19.getCO2Raw(true)));
        getDataTimer = millis();
    }
}
