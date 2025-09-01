#include <Arduino.h>
#include "src/include/Application.hpp"

Application application;

void setup() {
    application.init();
}

void loop() {
    application.loop();
}
