#pragma once

#include <stdint.h>

class StepperDriver {
public:
    virtual void init(bool inverted) = 0;

    virtual void setDirection(bool dir) = 0;

    virtual void step() = 0;

    virtual void enable(bool on) = 0;
};
