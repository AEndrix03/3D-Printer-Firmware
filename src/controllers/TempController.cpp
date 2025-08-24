#include <Arduino.h>
#include "../include/controllers/TempController.hpp"
#include "../include/Pins.hpp"
#include "../include/Config.hpp"
#include "../include/StateMachine.hpp"

namespace TempController {
    bool monitoringEnabled = true;
    float targetTemp = 0;

    void init() {
        pinMode(PIN_HEATER, OUTPUT);
        digitalWrite(PIN_HEATER, LOW);
    }

    void handle(uint8_t code, const char *params) {
        if (StateMachine::getState() == MachineState::Error && code != 0) {
            Serial.println(F("ERR TEMP_BLOCKED_ERROR_STATE"));
            return;
        }

        switch (code) {
            case 10: {
                // SET
                const char *p = strchr(params, 'T');
                if (p) {
                    float t = atof(p + 1);
                    setTargetTemperature(t);
                }
                break;
            }
            case 20: // GET
                Serial.print(F("TEMP="));
                Serial.println(getTemperature());
                break;
            case 0:
                setTargetTemperature(0);
                digitalWrite(PIN_HEATER, LOW);
                Serial.println(F("TEMP EMERGENCY STOP"));
                break;
            default:
                Serial.print(F("Unknown T-code: T"));
                Serial.println(code);
                break;
        }
    }

    float getTemperature() {
        if (!monitoringEnabled) return 22.0f; // default simulato
        int raw = analogRead(PIN_THERMISTOR);
        return (raw / 1023.0f) * TEMP_MAX;
    }


    void setTargetTemperature(float temp) {
        targetTemp = temp;
        if (targetTemp > 0) {
            digitalWrite(PIN_HEATER, HIGH);
        } else {
            digitalWrite(PIN_HEATER, LOW);
        }
    }

    bool isStable() {
        float t = getTemperature();
        return fabs(t - targetTemp) < TEMP_HYSTERESIS;
    }

    void enableMonitoring(bool enabled) {
        monitoringEnabled = enabled;
    }
}
