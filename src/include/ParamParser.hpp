#pragma once

#include <Arduino.h>

/**
 * @brief Lettura dei parametri "<lettera><numero>" di un comando (es. "X10.5 F1500").
 */
namespace ParamParser {
    /**
     * @param params stringa parametri (puo' essere nullptr)
     * @param key lettera del parametro
     * @param out valore letto; invariato se il parametro manca
     * @return true se il parametro e' presente
     */
    inline bool get(const char *params, char key, float &out) {
        if (!params) return false;
        const char *p = strchr(params, key);
        if (!p) return false;
        out = atof(p + 1);
        return true;
    }
}
