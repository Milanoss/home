#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <HTU21D.h>
// #include <SoftwareSerial.h>
#include "MHZ19.h"
#include "RunningMedian.h"

// #define RX_PIN 10      // Rx pin which the MHZ19 Tx pin is attached to
// #define TX_PIN 11      // Tx pin which the MHZ19 Rx pin is attached to
#define RX_PIN 13      // Rx pin which the MHZ19 Tx pin is attached to
#define TX_PIN 15      // Tx pin which the MHZ19 Rx pin is attached to
#define BAUDRATE 9600  // Native to the sensor (do not change)

const String wifi_name = "HOME";
const String wifi_pass = "12345678";

int interval = 100;
unsigned long previousMillis = 0;

HTU21D myHTU21D(HTU21D_RES_RH12_TEMP14);

MHZ19 myMHZ19;                            // Constructor for MH-Z19 class
// SoftwareSerial mySerial(RX_PIN, TX_PIN);  // Uno example
HardwareSerial mySerial(1); 

unsigned long getDataTimer = 0;  // Variable to store timer interval
unsigned long sendTimer = 0;
int CO2;
float temperature = 0;
float humidity = 0;

RunningMedian co2samples = RunningMedian(7);
RunningMedian tempSamples = RunningMedian(7);
RunningMedian humSamples = RunningMedian(7);

void WIFI_Connect() {
    WiFi.disconnect();
    // Serial.println("Connecting to WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_name, wifi_pass);

    for (int i = 0; i < 60; i++) {
        if (WiFi.status() != WL_CONNECTED) {
            delay(250);
            // Serial.print(".");
            delay(250);
        }
    }
    if (WiFi.status() == WL_CONNECTED) {
        // Serial.println("");
        // Serial.println("WiFi Connected");
        // Serial.println("IP address: ");
        // Serial.println(WiFi.localIP());
    }
}

void sendData() {
    IPAddress ip = WiFi.gatewayIP();
    WiFiClient client;
    HTTPClient httpClient1;

    float medianCo2 = co2samples.getMedian();
    float medianHum = humSamples.getMedian();
    float medianTemp = tempSamples.getMedian();
    if (isnan(medianCo2) || isnan(medianHum) || isnan(medianTemp)) {
        return;
    }
    String co2 = String((int)medianCo2);
    String hum = String((int)medianHum);
    String temp = String(medianTemp);
    String url = "http://" + ip.toString() + "/api/sensor?CO2=" + co2 + "&HUM=" + hum + "&TEMP=" + temp;
    // Serial.println(url);
    if (httpClient1.begin(client, url)) {
        int httpCode = httpClient1.GET();
        // Serial.printf("Sending code: %d\n", httpCode);
        String payload = httpClient1.getString();
        if (httpCode == HTTP_CODE_OK) {
            // Serial.print("Data sent");
        } else {
            // Serial.println("Cannot send data!");
        }
        httpClient1.end();
    } else {
        // Serial.println("[HTTP} Unable to connect");
    }
}

void printErrorCode(){
    // Serial.println("Communication error. Error Code: ");  // *Print error code using the library variable
    // Serial.println(myMHZ19.errorCode);                    //  holds the last recieved code
}  

void setRange(int range){
    // Serial.println("Setting range..");

    myMHZ19.setRange(range);                                               // request new range write

    // if ((myMHZ19.errorCode == RESULT_OK) && (myMHZ19.getRange() == range)) //RESULT_OK is an alias from the library,
        // Serial.println("Range successfully applied.");

    // else
    // {
        // printErrorCode();
    // }
}

void setup() {
    Serial.begin(9600);  // For ESP32 baudarte is 115200 etc.

    // mySerial.begin(BAUDRATE);

    // mySerial.begin(BAUDRATE, SERIAL_8N1, RX_PIN, TX_PIN); // ESP32 Example

    myMHZ19.begin(Serial);
    Serial.swap();

    setRange(2000);

    myMHZ19.autoCalibration(false);

    while (myHTU21D.begin() != true) {
        // Serial.print(F("HTU21D error"));
        delay(5000);
    }

    WIFI_Connect();

    // delay(120000);
}

void validate(int CO2){
    if (CO2 < 390 || CO2 > 2000)
        {
            Serial.println("Waiting for verification....");

                Serial.println("Failed to verify.");
                Serial.println("Requesting MHZ19 reset sequence");

                myMHZ19.recoveryReset();                                            // Recovery Reset

                Serial.println("Restarting MHZ19.");
                Serial.println("Waiting for boot duration to elapse.....");

                delay(30000);       
                
                Serial.println("Waiting for boot verification...");

        }
}

void loop() {
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval) {
        if (WiFi.status() != WL_CONNECTED) {
            // Serial.println("wifi disconnected ");
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
    if (millis() - getDataTimer >= 20000) {
        humidity = myHTU21D.readCompensatedHumidity();
        temperature = myHTU21D.readTemperature();
        // Serial.print("Temperature (C): ");
        // Serial.println(temperature);
        // Serial.print("Humidity (%): ");
        // Serial.println(humidity);
        if (temperature != HTU21D_ERROR) {
            tempSamples.add(temperature);
        }
        if (humidity != HTU21D_ERROR) {
            humSamples.add(humidity);
        }

        /* note: getCO2() default is command "CO2 Unlimited". This returns the correct CO2 reading even 
        if below background CO2 levels or above range (useful to validate sensor). You can use the 
        usual documented command with getCO2(false) */

        CO2 = myMHZ19.getCO2();
        // Serial.print("CO2 (ppm): ");
        // Serial.println(CO2);
        // validate(CO2);
        // Temp = myMHZ19.getTemperature(true, true);
        if (myMHZ19.errorCode == RESULT_OK && CO2 > 0 && CO2 < 5000) {
            co2samples.add(CO2);
        }
        // Serial.println("getCO2(false)" + String(myMHZ19.getCO2(false)));
        // Serial.println("getCO2(true)" + String(myMHZ19.getCO2(true)));
        // Serial.println("getCO2Raw(false)" + String(myMHZ19.getCO2Raw(false)));
        // Serial.println("getCO2Raw(true)" + String(myMHZ19.getCO2Raw(true)));
        getDataTimer = millis();
    }

    if (millis() - sendTimer >= 120000) {
        sendData();
        sendTimer = millis();
    }
}
