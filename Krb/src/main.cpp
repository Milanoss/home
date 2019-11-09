#include <Arduino.h>
#include "LowPower.h"

int led = 6;

void setup() {
    // initialize digital pin LED_BUILTIN as an output.
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(led, OUTPUT);
    Serial.begin(115200);
}

// the loop function runs over and over again forever
void loop() {
    Serial.println("loop");
    digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
    digitalWrite(led, HIGH);
    delay(1000);                      // wait for a second
    digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
    digitalWrite(led, LOW); 
    delay(1000);                      // wait for a second
    // LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
}