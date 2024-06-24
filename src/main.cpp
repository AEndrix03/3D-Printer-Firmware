#include <Arduino.h>
#include <string>

void setup() {
    std::string example = "Hello, World!";
    Serial.begin(9600);
    Serial.println(example.c_str());
}

void loop() {
    // put your main code here, to run repeatedly:
}