#include <Arduino.h>
#include "../include/controllers/TempController.hpp"
#include "../include/Pins.hpp"
#include "../include/Config.hpp"
#include "../include/SafetyManager.hpp"
#include "../include/StateMachine.hpp"
#include "../include/TimeUtils.hpp"
#include "../include/CompactResponse.hpp"

#define MAX_TEMP_C 280
#define RUNAWAY_DELTA_C 15
#define HEATING_TIMEOUT_S 40
#define MIN_TEMP_RISE_C 4
#define THERMISTOR_ERROR_C -10

namespace TempController {
    bool monitoringEnabled = true;
    float targetTemp = 0;

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

        if (temp > MAX_TEMP_C) {
            CompactResponse::sendData("ALM", "MAX_TEMP");
            return true;
        }

        if (targetTemp > 0 && temp > (targetTemp + RUNAWAY_DELTA_C)) {
            CompactResponse::sendData("ALM", "RUNAWAY");
            return true;
        }

        if (heaterOn) {
            if (!(tp.flags & 0x01)) {
                tp.heaterStartTime = now;
                tp.tempAtStart_x10 = (int16_t) (temp * 10);
                tp.flags |= 0x01;
            } else if (TimeUtils::hasElapsed(tp.heaterStartTime, tp.timeoutSeconds * 1000UL)) {
                float rise = temp - (tp.tempAtStart_x10 / 10.0f);
                if (rise < MIN_TEMP_RISE_C) {
                    CompactResponse::sendData("ALM", "HEAT_TIMEOUT");
                    return true;
                }
                tp.heaterStartTime = now;
                tp.tempAtStart_x10 = (int16_t) (temp * 10);
            }
        } else {
            tp.flags &= ~0x01;
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
            CompactResponse::sendNoCommand(CompactResponse::BLOCKED_TEMP_ERROR);
            return;
        }

        switch (code) {
            case 10: {
                const char *p = strchr(params, 'T');
                if (p) {
                    float temp = atof(p + 1);
                    setTargetTemperature(temp);
                    char tempData[16];
                    snprintf(tempData, sizeof(tempData), "SET_%.1f", temp);
                    CompactResponse::sendData("TMP", tempData);
                }
                break;
            }
            case 20: {
                char tempData[16];
                snprintf(tempData, sizeof(tempData), "%.1f %.1f", getTemperature(), targetTemp);
                CompactResponse::sendData("TMP", tempData);
                break;
            }
            case 25: {
                char protData[8];
                snprintf(protData, sizeof(protData), "%s", (tp.flags & 0x02) ? "ON" : "OFF");
                CompactResponse::sendData("PRT", protData);
                break;
            }
            case 30: {
                const char *p = strchr(params, 'S');
                if (p) {
                    bool enable = atoi(p + 1) != 0;
                    if (enable) tp.flags |= 0x02;
                    else tp.flags &= ~0x02;

                    char protData[8];
                    snprintf(protData, sizeof(protData), "%s", enable ? "ON" : "OFF");
                    CompactResponse::sendData("PRT", protData);
                }
                break;
            }
            case 0:
                setTargetTemperature(0);
                CompactResponse::sendData("TMP", "OFF");
                break;
            default:
                CompactResponse::sendData("ERR", "UNK_TCODE");
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
        static uint32_t lastCheck = 0;

        if (TimeUtils::hasElapsed(lastCheck, 500)) {
            if (checkThermalRunaway()) {
                SafetyManager::emergencyStop("THERMAL_RUNAWAY");
            }
            lastCheck = millis();
        }
    }
}