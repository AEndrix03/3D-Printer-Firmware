//
// Created by redeg on 24/06/2024.
//

#ifndef INC_3D_PRINTER_FIRMWARE_COMMANDLISTENER_H
#define INC_3D_PRINTER_FIRMWARE_COMMANDLISTENER_H

#include "io/executors/CommandExecutor.h"

class CommandListener {
public:
    static CommandListener* getInstance();
    ~CommandListener();
private:
    static CommandListener* instance;
    CommandExecutor* commandExecutors;
};


#endif //INC_3D_PRINTER_FIRMWARE_COMMANDLISTENER_H
