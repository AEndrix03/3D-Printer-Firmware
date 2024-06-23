#ifndef END_STOP_EVENT_H
#define END_STOP_EVENT_H

#include <Arduino.h>

class EndStopEvent {
public:
    EndStopEvent(int pin);
    void update();
    void stop();
    void run();
    void setOnHighFunction(void (*function)());
    void setOnLowFunction(void (*function)());

private:
    int pin;
    bool state;
    bool lastState;
    void (*onHighFunction)();
    void (*onLowFunction)();
};

#endif
