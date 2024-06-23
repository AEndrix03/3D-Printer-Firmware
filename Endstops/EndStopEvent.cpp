#include "EndStopEvent.h"

EndStopEvent::EndStopEvent(int pin) : pin(pin), state(false), lastState(false), onHighFunction(nullptr), onLowFunction(nullptr) {
    pinMode(pin, INPUT);
    attachInterrupt(digitalPinToInterrupt(pin), [this]() { this->update(); }, CHANGE);
}

void EndStopEvent::update() {
    state = digitalRead(pin);

    if (state != lastState) {
        if (state == HIGH)
            if (onHighFunction)
                onHighFunction();
        else
            if (onLowFunction)
                onLowFunction();

        lastState = state;
    }
}

void EndStopEvent::stop() {
    detachInterrupt(digitalPinToInterrupt(pin));
}

void EndStopEvent::run() {
    attachInterrupt(digitalPinToInterrupt(pin), [this]() { this->update(); }, CHANGE);
}

void EndStopEvent::setOnHighFunction(void (*function)()) {
    this->onHighFunction = function;
}

void EndStopEvent::setOnLowFunction(void (*function)()) {
    this->onLowFunction = function;
}
