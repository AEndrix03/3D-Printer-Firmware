//
// Created by redeg on 24/06/2024.
//

#include "CommandFactory.h"

template <typename T>
Command &CommandFactory::createCommand(String command) {
    if (command.length() == 0) {
        //simula l'eccezione del notifier
    }

    int identifierSeparator = command.indexOf(' ');
    String identifier;

    if (identifierSeparator != -1) {
        identifier = command.substring(0, identifierSeparator-1);
    } else {
        identifier = command;
    }

    do {
        identifierSeparator = command.indexOf(' ');
        int identifierEqual = command.indexOf('=');

        String argumentKey = command.substring(0, identifierEqual-1);
        String argumentValue = command.substring(identifierEqual+1, identifierSeparator-1);

        if (identifierSeparator + 1 < command.length()) {
            command = command.substring(identifierSeparator + 1, command.length());
        }

    } while (command.indexOf(' ') != -1);

    if (T inst)
    //Chiama la execute del serial la quale controlla che comando fare
}



