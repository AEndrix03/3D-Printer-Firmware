//
// Created by redeg on 27/04/2025.
//

#pragma once


#include <Arduino.h>

namespace BusyHandler {

    /**
     * Abilita l'invio periodico del messaggio "BUSY".
     */
    void start();

    /**
     * Disabilita l'invio periodico del messaggio "BUSY".
     */
    void stop();

    /**
     * Deve essere chiamato regolarmente nel loop principale.
     * Se attivo, stampa "BUSY" ogni secondo.
     */
    void update();

} // namespace BusyHandler



