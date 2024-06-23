//
// Created by redeg on 25/08/2023.
//

#include "SerialListener.h"

void SerialListener::read() {
    if (Serial.available()) {
        // Leggi il byte ricevuto dalla porta seriale
        char receivedByte = Serial.read();

        digitalWrite(LED_BUILTIN, HIGH);
        delay(250);
        digitalWrite(LED_BUILTIN, LOW);

        // Fai qualcosa con il byte ricevuto
        // Ad esempio, puoi stamparlo sulla console seriale
        Serial.print("Byte ricevuto: ");
        Serial.println(receivedByte);
    }
}