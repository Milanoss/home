#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <HTU21D.h>
#include <U8g2lib.h>
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

MHZ19 myMHZ19;  // Constructor for MH-Z19 class
// SoftwareSerial mySerial(RX_PIN, TX_PIN);  // Uno example
// HardwareSerial mySerial(1);

unsigned long getDataTimer = 0;  // Variable to store timer interval
unsigned long sendTimer = 0;
int CO2;
float temperature = 0;
float humidity = 0;

RunningMedian co2samples = RunningMedian(7);
RunningMedian tempSamples = RunningMedian(7);
RunningMedian humSamples = RunningMedian(7);

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);  // Adafruit ESP8266/32u4/ARM Boards + FeatherWing OLED

static unsigned char teplota[] = {
    0x80,
    0x03,
    0xC0,
    0x07,
    0xE0,
    0x04,
    0xE0,
    0x07,
    0xE0,
    0x04,
    0xE0,
    0x07,
    0xE0,
    0x04,
    0xE0,
    0x07,
    0xE0,
    0x04,
    0xF0,
    0x0F,
    0xF8,
    0x1F,
    0xF8,
    0x1F,
    0xF8,
    0x1F,
    0xF8,
    0x1F,
    0xF0,
    0x0F,
    0xE0,
    0x07,
};
static unsigned char kapka[] = {
    0x00,
    0x00,
    0x80,
    0x01,
    0xC0,
    0x03,
    0xE0,
    0x07,
    0xF0,
    0x0F,
    0xF0,
    0x0F,
    0xF8,
    0x1F,
    0xF8,
    0x1B,
    0xFC,
    0x39,
    0xFC,
    0x39,
    0xFC,
    0x38,
    0x78,
    0x1C,
    0xF8,
    0x1F,
    0xF0,
    0x0F,
    0xC0,
    0x03,
    0x00,
    0x00,
};

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

void printErrorCode() {
    // Serial.println("Communication error. Error Code: ");  // *Print error code using the library variable
    // Serial.println(myMHZ19.errorCode);                    //  holds the last recieved code
}

void setRange(int range) {
    // Serial.println("Setting range..");

    myMHZ19.setRange(range);  // request new range write

    // if ((myMHZ19.errorCode == RESULT_OK) && (myMHZ19.getRange() == range)) //RESULT_OK is an alias from the library,
    // Serial.println("Range successfully applied.");

    // else
    // {
    // printErrorCode();
    // }
}

void show() {
    float medianCo2 = co2samples.getMedian();
    float medianHum = humSamples.getMedian();
    float medianTemp = tempSamples.getMedian();
    if (isnan(medianCo2)) {
        medianCo2 = 0;
    }
    if (isnan(medianHum)) {
        medianHum = 0;
    }
    if (isnan(medianTemp)) {
        medianTemp = 0;
    }

    char tempStr[10];
    sprintf(tempStr, "%0.1f", medianTemp);
    String tempStr2 = String(tempStr) + "°C";

    String humiStr = String((int)medianHum) + "%";

    String co2Str = String((int)medianCo2) + " ppm";

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_9x18B_mf);
    u8g2.drawXBM(0, 0, 16, 16, teplota);
    u8g2.drawUTF8(17, 13, tempStr2.c_str());

    int width = u8g2.getDisplayWidth();
    int x = width - u8g2.getUTF8Width(humiStr.c_str());
    u8g2.drawXBM(width - 16, 0, 16, 16, kapka);
    u8g2.drawUTF8(x - 16, 13, humiStr.c_str());

    u8g2.setFont(u8g2_font_ImpactBits_tr);
    x = (width - u8g2.getUTF8Width(co2Str.c_str())) / 2;
    u8g2.drawUTF8(x, 28, co2Str.c_str());
    u8g2.drawFrame(0, 0, width, u8g2.getDisplayHeight());
    u8g2.sendBuffer();
}

void setup() {
    Serial.begin(9600);  // For ESP32 baudarte is 115200 etc.
    // Serial.println("setup1");

    // mySerial.begin(BAUDRATE);

    // mySerial.begin(BAUDRATE, SERIAL_8N1, RX_PIN, TX_PIN); // ESP32 Example

    myMHZ19.begin(Serial);
    Serial.swap();

    setRange(2000);

    myMHZ19.autoCalibration(false);

    while (myHTU21D.begin() != true) {
        Serial.print(F("HTU21D error"));
        delay(5000);
    }

    WIFI_Connect();

    u8g2.begin();
    u8g2.enableUTF8Print();

    // delay(120000);
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
        show();

        getDataTimer = millis();
    }

    if (millis() - sendTimer >= 120000) {
        sendData();
        sendTimer = millis();
    }
}
