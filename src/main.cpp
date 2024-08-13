#include <Arduino.h>
#include "io/command/Command.h"

void setup() {
    Serial.begin(9600);
}

void loop() {
    while(Serial.available()) {
        String cmd = "";
        char in;
        do {
            in = (char)Serial.read();
            if (in != ';')
                cmd += in;
        } while(in != ';');
        auto* command = new Command(cmd);
        command->execute();
        delete command;
    }
}