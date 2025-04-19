#include <Arduino.h>
#include "../include/controllers/MotionController.hpp"
#include "../include/Pins.hpp"

namespace MotionController {

    void init() {
        pinMode(PIN_X_STEP, OUTPUT);
        pinMode(PIN_X_DIR, OUTPUT);
        pinMode(PIN_Y_STEP, OUTPUT);
        pinMode(PIN_Y_DIR, OUTPUT);
    }

    void moveTo(float x, float y, float f) {
        // Movimento simulato con stampa su seriale (nessuna interpolazione o stepper reale)
        Serial.print(F("MOVE to X="));
        Serial.print(x);
        Serial.print(F(" Y="));
        Serial.print(y);
        Serial.print(F(" F="));
        Serial.println(f);
    }

    void emergencyStop() {
        // Spegni output / stop istantaneo
        digitalWrite(PIN_X_STEP, LOW);
        digitalWrite(PIN_Y_STEP, LOW);
        Serial.println(F("EMERGENCY STOP triggered"));
    }

    void handle(int code, const char *params) {
        switch (code) {
            case 10: {
                // Parse X, Y, F
                float x = 0.0f, y = 0.0f, f = 1000.0f;
                const char *px = strchr(params, 'X');
                const char *py = strchr(params, 'Y');
                const char *pf = strchr(params, 'F');
                if (px) x = atof(px + 1);
                if (py) y = atof(py + 1);
                if (pf) f = atof(pf + 1);
                moveTo(x, y, f);
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
