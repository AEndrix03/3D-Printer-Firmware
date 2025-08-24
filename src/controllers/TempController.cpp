// TempController.cpp - RAM Optimized Thermal Protection

#include <Arduino.h>
#include "../include/controllers/TempController.hpp"
#include "../include/Pins.hpp"
#include "../include/Config.hpp"
#include "../include/SafetyManager.hpp"
#include "../include/StateMachine.hpp"

// Constanti in PROGMEM per risparmiare RAM
#define MAX_TEMP_C 280
#define RUNAWAY_DELTA_C 15
#define HEATING_TIMEOUT_S 40
#define MIN_TEMP_RISE_C 4
#define THERMISTOR_ERROR_C -10

namespace TempController {
    bool monitoringEnabled = true;
    float targetTemp = 0;

    // Struttura ottimizzata: 10 bytes vs 18 bytes originali
    struct ThermalProtection {
        uint32_t heaterStartTime; // 4 bytes
        int16_t tempAtStart_x10; // 2 bytes (temperatura * 10)
        uint16_t timeoutSeconds; // 2 bytes
        uint8_t flags; // 1 byte: bit0=heaterWasOn, bit1=protectionEnabled
    };

    static ThermalProtection tp = {0, 0, HEATING_TIMEOUT_S, 0x02}; // protectionEnabled=true

    void init() {
        pinMode(PIN_HEATER, OUTPUT);
        digitalWrite(PIN_HEATER, LOW);
        tp = {0, 0, HEATING_TIMEOUT_S, 0x02};
    }

    float getTemperature() {
        if (!monitoringEnabled) return 22.0f;

        int raw = analogRead(PIN_THERMISTOR);
        if (raw == 0 || raw == 1023) {
            if (targetTemp > 0) SafetyManager::emergencyStop("THERMISTOR_ERR");
            return THERMISTOR_ERROR_C;
        }
        return (raw / 1023.0f) * TEMP_MAX;
    }

    bool checkThermalRunaway() {
        if (!(tp.flags & 0x02) || !monitoringEnabled) return false;

        float temp = getTemperature();
        bool heaterOn = (digitalRead(PIN_HEATER) == HIGH);
        uint32_t now = millis();

        // Max temp check
        if (temp > MAX_TEMP_C) {
            Serial.println(F("!!! MAX_TEMP"));
            return true;
        }

        // Runaway check  
        if (targetTemp > 0 && temp > (targetTemp + RUNAWAY_DELTA_C)) {
            Serial.println(F("!!! RUNAWAY"));
            return true;
        }

        // Heating timeout check
        if (heaterOn) {
            if (!(tp.flags & 0x01)) {
                // heaterWasOn == false
                tp.heaterStartTime = now;
                tp.tempAtStart_x10 = (int16_t) (temp * 10);
                tp.flags |= 0x01; // Set heaterWasOn
            } else if ((now - tp.heaterStartTime) >= (tp.timeoutSeconds * 1000UL)) {
                float rise = temp - (tp.tempAtStart_x10 / 10.0f);
                if (rise < MIN_TEMP_RISE_C) {
                    Serial.println(F("!!! HEAT_TIMEOUT"));
                    return true;
                }
                tp.heaterStartTime = now;
                tp.tempAtStart_x10 = (int16_t) (temp * 10);
            }
        } else {
            tp.flags &= ~0x01; // Clear heaterWasOn
        }

        return false;
    }

    void setTargetTemperature(float temp) {
        targetTemp = temp;
        if (StateMachine::getState() == MachineState::Error) return;

        digitalWrite(PIN_HEATER, temp > 0 ? HIGH : LOW);
        if (temp <= 0) {
            tp.heaterStartTime = 0;
            tp.flags &= ~0x01;
        }
    }

    void handle(uint8_t code, const char *params) {
        if (StateMachine::getState() == MachineState::Error && code != 0 && code != 20) {
            Serial.println(F("ERR BLOCKED"));
            return;
        }

        switch (code) {
            case 10: {
                const char *p = strchr(params, 'T');
                if (p) setTargetTemperature(atof(p + 1));
                break;
            }
            case 20:
                Serial.print(F("TEMP="));
                Serial.print(getTemperature());
                Serial.print(F(" TGT="));
                Serial.println(targetTemp);
                break;
            case 25:
                Serial.print(F("PROT="));
                Serial.println((tp.flags & 0x02) ? F("ON") : F("OFF"));
                break;
            case 30: {
                const char *p = strchr(params, 'S');
                if (p) {
                    if (atoi(p + 1)) tp.flags |= 0x02;
                    else tp.flags &= ~0x02;
                }
                break;
            }
            case 0:
                setTargetTemperature(0);
                break;
        }
    }

    bool isStable() {
        return fabs(getTemperature() - targetTemp) < TEMP_HYSTERESIS;
    }

    void enableMonitoring(bool enabled) {
        monitoringEnabled = enabled;
    }

    void updateThermalProtection() {
        static uint16_t lastCheck = 0;
        uint16_t now = millis() & 0xFFFF;

        if ((uint16_t) (now - lastCheck) >= 500) {
            if (checkThermalRunaway()) {
                SafetyManager::emergencyStop("THERMAL_RUNAWAY");
            }
            lastCheck = now;
        }
    }
}
