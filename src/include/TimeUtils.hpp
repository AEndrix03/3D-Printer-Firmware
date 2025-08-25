//
// Created by Andrea on 25/08/2025.
//

#pragma once
#include <Arduino.h>

namespace TimeUtils {
    /**
     * @brief Controlla se è passato un certo intervallo (overflow-safe)
     * @param start Timestamp di partenza
     * @param interval Intervallo in millisecondi
     * @return true se interval è trascorso da start
     */
    inline bool hasElapsed(uint32_t start, uint32_t interval) {
        return (uint32_t) (millis() - start) >= interval;
    }

    /**
     * @brief Calcola tempo trascorso (overflow-safe)
     * @param start Timestamp di partenza
     * @return Millisecondi trascorsi
     */
    inline uint32_t elapsed(uint32_t start) {
        return millis() - start;
    }
}
