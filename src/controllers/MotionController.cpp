#include <Arduino.h>
#include "../include/controllers/MotionController.hpp"
#include "../include/Pins.hpp"
#include "../include/hal/drivers/stepper/A4988Stepper.hpp"

namespace {
    A4988Stepper stepperX(PIN_X_STEP, PIN_X_DIR);
    A4988Stepper stepperY(PIN_Y_STEP, PIN_Y_DIR);
    A4988Stepper stepperZ(PIN_Z_STEP, PIN_Z_DIR);
}

namespace MotionController {

    void init() {
        stepperX.init();
        stepperY.init();
        stepperZ.init();
        stepperX.enable(true);
        stepperY.enable(true);
        stepperZ.enable(true);
    }

    void moveTo(float x, float y, float z, float f) {
        // Movimento dimostrativo: 100 step per asse
        Serial.print(F("MOVE to X="));
        Serial.print(x);
        Serial.print(F(" Y="));
        Serial.print(y);
        Serial.print(F(" Z="));
        Serial.print(z);
        Serial.print(F(" F="));
        Serial.println(f);

        stepperX.setDirection(x >= 0);
        stepperY.setDirection(y >= 0);
        stepperZ.setDirection(z >= 0);

        int steps = 100;  // test
        for (int i = 0; i < steps; ++i) {
            stepperX.step();
            stepperY.step();
            stepperZ.step();
            delayMicroseconds(100);  // temporizzazione base
        }
    }

    void emergencyStop() {
        stepperX.enable(false);
        stepperY.enable(false);
        Serial.println(F("EMERGENCY STOP triggered"));
    }

    void handle(int code, const char *params) {
        switch (code) {
            case 10: {
                float x = 0.0f, y = 0.0f, z = 0.0f, f = 1000.0f;
                const char *px = strchr(params, 'X');
                const char *py = strchr(params, 'Y');
                const char *pz = strchr(params, 'Z');
                const char *pf = strchr(params, 'F');
                if (px) x = atof(px + 1);
                if (py) y = atof(py + 1);
                if (py) z = atof(pz + 1);
                if (pf) f = atof(pf + 1);
                moveTo(x, y, z, f);
                break;
            }
            case 0:
                emergencyStop();
                break;
            default:
                Serial.print(F("Unknown M-code: M"));
                Serial.println(code);
                break;
        }
    }

}
