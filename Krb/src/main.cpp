#include <DallasTemperature.h>
#include <OneWire.h>
#include <TM1637Display.h>
#include "LowPower.h"

#define CLK 2
#define DIO 3

#define SPEAKER 6

const int maxTemp = 23;

boolean alarm;

const int buttonPin = 5;
// nastavení čísla vstupního pinu
const int pinCidlaDS = 4;
// vytvoření instance oneWireDS z knihovny OneWire
OneWire oneWireDS(pinCidlaDS);
// vytvoření instance senzoryDS z knihovny DallasTemperature
DallasTemperature senzoryDS(&oneWireDS);

const uint8_t AHOJ[] = {
    SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,  // A
    SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,          // H
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,  // O
    SEG_B | SEG_C | SEG_D | SEG_E                   // J
};
// vytvoření instance displej z knihovny TM1637
TM1637Display displej(CLK, DIO);

void alarmFire() {
    tone(SPEAKER, 1000);
    delay(250);
    noTone(SPEAKER);
    delay(250);
}

void showTemp(float temp) {
    int num = temp * 100;
    if (temp > 9999) {
        num = 9999;
    }
    displej.setBrightness(10);
    displej.showNumberDecEx(num, 0b01000000);
}

void setup(void) {
    // komunikace přes sériovou linku rychlostí 9600 baud
    // Serial.begin(9600);
    delay(1000);
    pinMode(SPEAKER, OUTPUT);
    // zapnutí komunikace knihovny s teplotním čidlem
    senzoryDS.begin();

    // noInterrupts();
    // CLKPR = _BV(CLKPCE);  // enable change of the clock prescaler
    // CLKPR = _BV(CLKPS0);  // divide frequency by 2
    // interrupts();

    displej.setBrightness(1);
    // výpis vlastního slova AHOJ
    displej.setSegments(AHOJ);

    senzoryDS.requestTemperatures();
    float temp = senzoryDS.getTempCByIndex(0);
    showTemp(temp);

    delay(2000);
    displej.clear();
    displej.setBrightness(0, false);  // Turn off
}

void loop(void) {
    // načtení informací ze všech připojených čidel na daném pinu
    senzoryDS.requestTemperatures();
    // výpis teploty na sériovou linku, při připojení více čidel
    // na jeden pin můžeme postupně načíst všechny teploty
    // pomocí změny čísla v závorce (0) - pořadí dle unikátní adresy čidel
    float temp = senzoryDS.getTempCByIndex(0);
    // Serial.print("Teplota cidla DS18B20: ");
    // Serial.print(temp);
    // Serial.println(" stupnu Celsia");
    // pauza pro přehlednější výpis
    alarm = maxTemp < temp;
    if (alarm) {
        displej.setBrightness(1, true);
        showTemp(temp);
        alarmFire();
        return;
    }

    if (digitalRead(buttonPin) == HIGH || alarm) {
        displej.setBrightness(1, true);
        showTemp(temp);
        delay(2000);
        displej.clear();
        displej.setBrightness(0, false);  // Turn off
    }

    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    // LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
}
// #include <Arduino.h>
// #include "LowPower.h"

// int led = 6;

// void setup() {
//     // initialize digital pin LED_BUILTIN as an output.
//     pinMode(LED_BUILTIN, OUTPUT);
//     pinMode(led, OUTPUT);
//     Serial.begin(115200);
// }

// // the loop function runs over and over again forever
// void loop() {
//     Serial.println("loop");
//     digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
//     digitalWrite(led, HIGH);
//     delay(1000);                      // wait for a second
//     digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
//     digitalWrite(led, LOW);
//     delay(1000);                      // wait for a second
//     LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
//     LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
//     LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
// }