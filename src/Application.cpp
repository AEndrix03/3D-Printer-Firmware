//
// Created by redeg on 17/04/2025.
//

#include <Arduino.h>
#include "Application.h"
extern HardwareSerial Serial;

void Application::init() {
    Serial.begin(19200);
    while (!Serial);
    Serial.println("Seriale pronta!");

    pinMode(13, OUTPUT);
}

void Application::loop() {
    Serial.println("High!");
    digitalWrite(13, HIGH);
    delay(100);
    Serial.println("Low!");
    digitalWrite(13, LOW);
    delay(100);
}

