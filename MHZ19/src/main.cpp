#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>  // Remove if using HardwareSerial or non-uno library compatable device
#include "MHZ19.h"  // include main library
#include "RunningMedian.h"

// #define RX_PIN 10      // Rx pin which the MHZ19 Tx pin is attached to
// #define TX_PIN 11      // Tx pin which the MHZ19 Rx pin is attached to
#define RX_PIN 4       // Rx pin which the MHZ19 Tx pin is attached to
#define TX_PIN 5       // Tx pin which the MHZ19 Rx pin is attached to
#define BAUDRATE 9600  // Native to the sensor (do not change)

#define CLK 2
#define DIO 3
const String wifi_name = "TP-LINK_F092";
const String wifi_pass = "1takovenormalnipripojeni2takovenormalnipripojeni3#";

int interval = 100;
unsigned long previousMillis = 0;

MHZ19 myMHZ19;                            // Constructor for MH-Z19 class
SoftwareSerial mySerial(RX_PIN, TX_PIN);  // Uno example
//HardwareSerial mySerial(1);                              // ESP32 Example

// TM1637Display displej(CLK, DIO);

unsigned long getDataTimer = 0;  // Variable to store timer interval
unsigned long sendTimer = 0;
String thingData[] = {"", "", "", "", "", "", "", ""};
int CO2;

RunningMedian co2samples = RunningMedian(70);

void WIFI_Connect() {
    WiFi.disconnect();
    Serial.println("Connecting to WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_name, wifi_pass);

    for (int i = 0; i < 60; i++) {
        if (WiFi.status() != WL_CONNECTED) {
            delay(250);
            // digitalWrite(ledPin, LOW);
            Serial.print(".");
            delay(250);
            // digitalWrite(ledPin, HIGH);
        }
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("");
        Serial.println("WiFi Connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
    }
    // digitalWrite(ledPin, 0);
}

void sendDataToThingspeak() {
    // WIFI_Connect();
    thingData[5] = String((int)co2samples.getMedian());
    co2samples.clear();

    if (WiFi.status() == WL_CONNECTED) {
        WiFiClient client;

        HTTPClient httpClient1;

        String url = "http://api.thingspeak.com/update?api_key=TRBAF4LV7U76SVST";
        url += thingData[0] != "" ? "&field1=" + thingData[0] : "";
        url += thingData[1] != "" ? "&field2=" + thingData[1] : "";
        url += thingData[2] != "" ? "&field3=" + thingData[2] : "";
        url += thingData[3] != "" ? "&field4=" + thingData[3] : "";
        url += thingData[4] != "" ? "&field5=" + thingData[4] : "";
        url += thingData[5] != "" ? "&field6=" + thingData[5] : "";
        url += thingData[6] != "" ? "&field7=" + thingData[6] : "";
        url += thingData[7] != "" ? "&field8=" + thingData[7] : "";
        Serial.println(url);
        if (httpClient1.begin(client, url)) {
            int httpCode = httpClient1.GET();
            Serial.printf("Thingspeak code: %d\n", httpCode);
            String payload = httpClient1.getString();
            Serial.println(payload);
            if (httpCode == HTTP_CODE_OK) {
                Serial.print("Data sent to Thingspeak: ");
                Serial.println(payload);
            } else {
                Serial.println("Cannot send data to Thingspeak!");
            }
            httpClient1.end();
        } else {
            Serial.printf("[HTTP} Unable to connect\n");
        }

        for (int i = 0; i < 8; i++) {
            thingData[i] = "";
        }
    }
    // WiFi.disconnect();
}

void setup() {
    Serial.begin(9600);  // For ESP32 baudarte is 115200 etc.

    mySerial.begin(BAUDRATE);  // Uno example: Begin Stream with MHZ19 baudrate

    //mySerial.begin(BAUDRATE, SERIAL_8N1, RX_PIN, TX_PIN); // ESP32 Example

    myMHZ19.begin(mySerial);  // *Important, Pass your Stream reference

    myMHZ19.autoCalibration(false);  // Turn auto calibration ON (disable with autoCalibration(false))

    // displej.setBrightness(10);

    // pinMode(7, INPUT_PULLUP);
    WIFI_Connect();
}

void loop() {
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("wifi disconnected ");
            WIFI_Connect();
        }
        // save the last time you blinked the LED
        previousMillis = currentMillis;
    }

    // int val = 0;
    // val = digitalRead(7);  // read input value
    // if (val == LOW) {     // check if the input is HIGH (button released)
    //     Serial.println("CALIBRATION");
    //     myMHZ19.calibrateZero();
    // }
    if (millis() - sendTimer >= 120000) {
        sendDataToThingspeak();
        sendTimer = millis();  // Update interval
    }
    if (millis() - getDataTimer >= 20000)  // Check if interval has elapsed (non-blocking delay() equivilant)
    {
        /* note: getCO2() default is command "CO2 Unlimited". This returns the correct CO2 reading even 
        if below background CO2 levels or above range (useful to validate sensor). You can use the 
        usual documented command with getCO2(false) */

        CO2 = myMHZ19.getCO2();  // Request CO2 (as ppm)
        // Serial.println("getCO2(false)" + String(myMHZ19.getCO2(false)));
        // Serial.println("getCO2(true)" + String(myMHZ19.getCO2(true)));
        // Serial.println("getCO2Raw(false)" + String(myMHZ19.getCO2Raw(false)));
        // Serial.println("getCO2Raw(true)" + String(myMHZ19.getCO2Raw(true)));

        Serial.print("CO2 (ppm): ");
        Serial.println(CO2);

        int8_t Temp;                      // Buffer for temperature
        Temp = myMHZ19.getTemperature();  // Request Temperature (as Celsius)
        Serial.print("Temperature (C): ");
        Serial.println(Temp);

        getDataTimer = millis();  // Update interval

        if (CO2 > 0 && CO2 < 5000) {
            co2samples.add(CO2);
        }
    }
}
