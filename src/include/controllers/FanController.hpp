#pragma once

#include <stdint.h>

/**
 * @brief Controlla le ventole PWM collegate alla macchina.
 */
namespace FanController {

    /**
     * @brief Inizializza i pin PWM e imposta lo stato iniziale.
     */
    void init();

    /**
     * @brief Esegue un comando di ventilazione.
     * @param code Codice del comando (es. 10 = SET SPEED)
     * @param params Parametri (es. "S255")
     */
    void handle(int code, const char *params);

    /**
     * @brief Imposta la velocità della ventola.
     * @param pwm Valore PWM da 0 (off) a 255 (max)
     */
    void setSpeed(uint8_t pwm);
}
