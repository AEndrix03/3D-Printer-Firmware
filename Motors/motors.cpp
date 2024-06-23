#include "motors.h"
#include "../Configurations/geometry.h"
#include "../Endstops/EndStopEvent.h"

void step(Motor motor, int steps, int dir) {
    int pinStep, pinDir, speed;

    if (steps == 0)
        return;
    else if (steps < 0) {
        steps *= -1;
        dir *= -1;
    }

    switch (motor) {
        case X: {
            dir *= X_INVERT_DIRECTION ? -1 : 1;
            speed = 1/(2*X_MAX_SPEED);
            pinStep = X_STEP;
            pinDir = X_DIR;
        }
        case Y: {
            dir *= Y_INVERT_DIRECTION ? -1 : 1;
            speed = 1/(2*Y_MAX_SPEED);
            pinStep = Y_STEP;
            pinDir = Y_DIR;
        }
        case Z: {
            dir *= Z_INVERT_DIRECTION ? -1 : 1;
            speed = 1/(2*Z_MAX_SPEED);
            pinStep = Z_STEP;
            pinDir = Z_DIR;
        }
        case A: {
            dir *= A_INVERT_DIRECTION ? -1 : 1;
            speed = 1/(2*A_MAX_SPEED);
            pinStep = A_STEP;
            pinDir = A_DIR;
        }
    }

    if (dir > 0)
        digitalWrite(pinDir, HIGH);
    else 
        digitalWrite(pinDir, LOW);

    speed *= 1000;

    for (int i = 0; x < steps; i++) {
        digitalWrite(pinStep, HIGH);
        delay(speed);
        digitalWrite(pinStep, LOW);
        delay(speed);
    }

}

void testEndStopXAxis() {
    EndStopEvent xEndStopEvent = new EndStopEvent(X_ENDSTOP);

    Serial.println("...");

    c = 0;
    dir = -1;
    steps = 0;

    void onHigh() {
        if (c == 0) {
            Serial.println("X Min EndStop touched. Start measuring...");
            dir = 1;
            steps = 0;
        } else {
            Serial.println("X Max EndStop touched. Finish measuring.");
            Serial.print("Steps: ");
            Serial.println(steps);
        }
        c++;
    }

    xEndStopEvent.setOnHighFunction(onHigh);

    while (true) {
        if (c > 1)
            break;
        step(X, 1, dir);
        steps++;
    }

    delete xEndStopEvent;
    xEndStopEvent = nullptr;

    Serial.println("...");
}

void testEndStopYAxis() {
    EndStopEvent yEndStopEvent = new EndStopEvent(Y_ENDSTOP);

    Serial.println("...");

    c = 0;
    dir = -1;
    steps = 0;

    void onHigh() {
        if (c == 0) {
            Serial.println("Y Min EndStop touched. Start measuring...");
            dir = 1;
            steps = 0;
        } else {
            Serial.println("Y Max EndStop touched. Finish measuring.");
            Serial.print("Steps: ");
            Serial.println(steps);
        }
        c++;
    }

    yEndStopEvent.setOnHighFunction(onHigh);

    while (true) {
        if (c > 1)
            break;
        step(Y, 1, dir);
        steps++;
    }

    delete yEndStopEvent;
    yEndStopEvent = nullptr;

    Serial.println("...");
}

void testEndStopZAxis() {
    EndStopEvent zEndStopEvent = new EndStopEvent(Z_ENDSTOP);

    Serial.println("...");

    c = 0;
    dir = -1;
    steps = 0;

    void onHigh() {
        if (c == 0) {
            Serial.println("Z Min EndStop touched. Start measuring...");
            dir = 1;
            steps = 0;
        } else {
            Serial.println("Z Max EndStop touched. Finish measuring.");
            Serial.print("Steps: ");
            Serial.println(steps);
        }
        c++;
    }

    zEndStopEvent.setOnHighFunction(onHigh);

    while (true) {
        if (c > 1)
            break;
        step(Z, 1, dir);
        steps++;
    }

    delete zEndStopEvent;
    zEndStopEvent = nullptr;

    Serial.println("...");
}
