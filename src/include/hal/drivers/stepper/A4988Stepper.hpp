#pragma once

#include "../StepperDriver.hpp"

class A4988Stepper : public StepperDriver {
public:
    A4988Stepper(uint8_t stepPin, uint8_t dirPin, uint8_t enPin = 255);

    void init(bool inverted) override;

    void setDirection(bool dir) override;

    void step() override;

    void enable(bool on) override;

private:
    uint8_t _step, _dir, _en, _inv;
};
