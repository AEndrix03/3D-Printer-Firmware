#include <Arduino.h>
#include "../include/controllers/TempController.hpp"
#include "../include/Pins.hpp"
#include "../include/Config.hpp"
#include "../include/SafetyManager.hpp"
#include "../include/StateMachine.hpp"
#include "../include/TimeUtils.hpp"
#include "../include/ParamParser.hpp"
#include "../include/CompactResponse.hpp"

namespace {
    constexpr uint8_t FLAG_REACHED = 0x01;    // target raggiunto almeno una volta
    constexpr uint8_t FLAG_PROTECTION = 0x02; // protezione runaway attiva
    constexpr uint8_t FLAG_VALID = 0x04;      // almeno una lettura valida
    constexpr uint16_t UPDATE_INTERVAL_MS = 250;

    bool monitoringEnabled = TEMP_MONITORING_ENABLED;
    bool sensorFault = false;
    float targetTemp = 0.0f;
    float currentTemp = 22.0f;

    uint8_t flags = FLAG_PROTECTION;
    uint32_t lastUpdate = 0;
    uint32_t watchStart = 0;
    float watchTemp = 0.0f;

    void heater(bool on) {
        digitalWrite(PIN_HEATER, on ? HIGH : LOW);
    }

    void restartWatch() {
        watchStart = millis();
        watchTemp = currentTemp;
        flags &= ~FLAG_REACHED;
    }

    /** Equazione Beta. Ritorna false se il sensore e' aperto o in corto. */
    bool readThermistor(float &out) {
        const int raw = analogRead(PIN_THERMISTOR);
        if (raw <= 3 || raw >= 1015) return false;

        const float r = THERMISTOR_PULLUP * (float) raw / (1023.0f - (float) raw);
        const float invT = (1.0f / 298.15f) + log(r / THERMISTOR_R25) / THERMISTOR_BETA;
        out = (1.0f / invT) - 273.15f;
        return true;
    }

    void sendTemperature() {
        CompactResponse::begin(PSTR("TMP"), true);
        CompactResponse::addFloat(currentTemp, 1);
        CompactResponse::addFloat(targetTemp, 1);
        CompactResponse::end();
    }
}

namespace TempController {
    void init() {
        pinMode(PIN_HEATER, OUTPUT);
        heater(false);
        targetTemp = 0.0f;
        flags = FLAG_PROTECTION;
    }

    float getTemperature() {
        return currentTemp;
    }

    float getTargetTemperature() {
        return targetTemp;
    }

    void forceOff() {
        heater(false);
        targetTemp = 0.0f;
    }

    bool setTargetTemperature(float temp) {
        if (temp <= 0.0f) {
            forceOff();
            return true;
        }
        if (!monitoringEnabled || sensorFault || temp > TEMP_MAX) return false;
        if (StateMachine::getState() == MachineState::Error) return false;

        targetTemp = temp;
        restartWatch();
        return true; // l'heater viene pilotato da update()
    }

    bool isStable() {
        return targetTemp > 0.0f && fabs(currentTemp - targetTemp) < TEMP_HYSTERESIS;
    }

    bool canExtrude() {
        return !monitoringEnabled || (!sensorFault && currentTemp >= TEMP_MIN_EXTRUDE);
    }

    void enableMonitoring(bool enabled) {
        monitoringEnabled = enabled;
        if (!enabled) forceOff();
    }

    void update() {
        if (!TimeUtils::hasElapsed(lastUpdate, UPDATE_INTERVAL_MS)) return;
        lastUpdate = millis();

        if (!monitoringEnabled) {
            heater(false);
            return;
        }

        float t;
        if (!readThermistor(t)) {
            sensorFault = true;
            heater(false);
            if (targetTemp > 0.0f) SafetyManager::emergencyStop(PSTR("THERM_ERR"));
            return;
        }
        sensorFault = false;

        // filtro passa-basso leggero contro il rumore dell'ADC
        if (flags & FLAG_VALID) {
            currentTemp += (t - currentTemp) * 0.25f;
        } else {
            currentTemp = t;
            flags |= FLAG_VALID;
        }

        if (currentTemp > TEMP_ABS_MAX) {
            SafetyManager::emergencyStop(PSTR("MAX_TEMP"));
            return;
        }

        if (targetTemp <= 0.0f || StateMachine::getState() == MachineState::Error) {
            heater(false);
            return;
        }

        // --- regolazione on/off con isteresi ---
        if (currentTemp < targetTemp - TEMP_HYSTERESIS / 2) {
            heater(true);
        } else if (currentTemp > targetTemp + TEMP_HYSTERESIS / 2) {
            heater(false);
        }

        // --- protezione thermal runaway ---
        if (!(flags & FLAG_PROTECTION)) return;

        const bool windowElapsed = TimeUtils::hasElapsed(watchStart, THERMAL_WATCH_PERIOD_S * 1000UL);

        if (!(flags & FLAG_REACHED)) {
            // fase di riscaldamento: deve salire di almeno THERMAL_MIN_RISE_C per finestra
            if (currentTemp >= targetTemp - TEMP_HYSTERESIS) {
                flags |= FLAG_REACHED;
                watchStart = millis();
            } else if (windowElapsed) {
                if (currentTemp - watchTemp < THERMAL_MIN_RISE_C) {
                    SafetyManager::emergencyStop(PSTR("HEAT_FAIL"));
                    return;
                }
                watchStart = millis();
                watchTemp = currentTemp;
            }
        } else {
            // a regime: non deve restare sotto il target troppo a lungo
            if (currentTemp >= targetTemp - THERMAL_RUNAWAY_DELTA_C) {
                watchStart = millis();
            } else if (windowElapsed) {
                SafetyManager::emergencyStop(PSTR("THERM_RUN"));
            }
        }
    }

    void handle(uint8_t code, const char *params) {
        if (StateMachine::getState() == MachineState::Error && code != 0 && code != 20) {
            CompactResponse::setResult(CompactResponse::BLOCKED_TEMP_ERROR);
            return;
        }

        switch (code) {
            case 10: {
                float temp;
                // "S" e' il parametro usato dal driver (M104 S200); "T" quello storico del contratto
                if (!ParamParser::get(params, 'S', temp) && !ParamParser::get(params, 'T', temp)) {
                    CompactResponse::reject(PSTR("NO_TEMP"));
                    break;
                }
                if (temp > 0.0f && !monitoringEnabled) {
                    CompactResponse::reject(PSTR("NO_SENSOR"));
                    break;
                }
                if (!setTargetTemperature(temp)) {
                    CompactResponse::reject(PSTR("BAD_TEMP"));
                    break;
                }
                sendTemperature();
                break;
            }
            case 20:
                sendTemperature();
                break;
            case 25:
                CompactResponse::sendCritical(PSTR("PRT"), (flags & FLAG_PROTECTION) ? PSTR("ON") : PSTR("OFF"));
                break;
            case 30: {
                float v;
                if (!ParamParser::get(params, 'S', v)) {
                    CompactResponse::reject(PSTR("NO_PARAM"));
                    break;
                }
                if (v != 0.0f) {
                    flags |= FLAG_PROTECTION;
                    restartWatch();
                } else {
                    flags &= ~FLAG_PROTECTION;
                }
                CompactResponse::sendCritical(PSTR("PRT"), (flags & FLAG_PROTECTION) ? PSTR("ON") : PSTR("OFF"));
                break;
            }
            case 0:
                forceOff();
                CompactResponse::sendData(PSTR("TMP"), PSTR("OFF"));
                break;
            default:
                CompactResponse::reject(PSTR("UNK_TCODE"));
                break;
        }
    }
}
