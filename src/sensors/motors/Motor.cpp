//
// Created by redeg on 12/08/2024.
//

#include "Motor.h"
#include "../SensorPinOut.h"
#include "../SensorsConfiguration.h"

void Motor::setup(SensorEnum motorType) {
    if (motorType == MOTOR_X) {
        stepPin = pins::X_STEP;
        dirPin = pins::X_DIR;
        speed = (1/(2*config::X_MAX_SPEED))*1000;
        dirVoltage = config::X_INVERT_DIRECTION ? LOW : HIGH;
        endStopPin = pins::X_ENDSTOP;
    } else if (motorType == MOTOR_Y) {
        stepPin = pins::Y_STEP;
        dirPin = pins::Y_DIR;
        speed = (1/(2*config::Y_MAX_SPEED))*1000;
        dirVoltage = config::Y_INVERT_DIRECTION ? LOW : HIGH;
        endStopPin = pins::Y_ENDSTOP;
    } else if (motorType == MOTOR_Z) {
        stepPin = pins::Z_STEP;
        dirPin = pins::Z_DIR;
        dirVoltage = config::Z_INVERT_DIRECTION ? LOW : HIGH;
        speed = (1/(2*config::Z_MAX_SPEED))*1000;
        endStopPin = pins::Z_ENDSTOP;
    } else if (motorType == MOTOR_A) {
        stepPin = pins::A_STEP;
        dirPin = pins::A_DIR;
        dirVoltage = config::A_INVERT_DIRECTION ? LOW : HIGH;
        speed = (1/(2*config::A_MAX_SPEED))*1000;
    }
}

bool Motor::doAction(char action) {
    if (digitalRead(endStopPin) == HIGH) {
        //Potrebbe essersi bloccato
        handleException();
        return false;
    }

    if (action == 1) {
        //Move up
        move(true);
    } else if (action == 2) {
        //Move down
        move(false);
    }

    return digitalRead(endStopPin) == HIGH;
}

void Motor::handleException() {
    if (config::ENABLE_ERRORS_LED_LOG) {
        digitalWrite(pins::ERROR_LED, HIGH);
        delay(config::ERROR_DELAY_MS);
        digitalWrite(pins::ERROR_LED, LOW);
        delay(config::ERROR_DELAY_MS);
    }
}

void Motor::move(bool up) {
    digitalWrite(dirPin, dirVoltage && up);
    digitalWrite(stepPin, HIGH);
    delay(speed);
    digitalWrite(stepPin, LOW);
    delay(speed);
}

Motor::Motor(SensorEnum motorType) {
    setup(motorType);
}
