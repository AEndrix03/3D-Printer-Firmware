//
// Created by redeg on 13/08/2024.
//

#include "Command.h"
#include "sensors/SensorMap.h"
#include "sensors/motors/Motor.h"

String Command::execute() {
    Sensor* s;
    SensorMap sm;
    SensorEnum en = sm.get(this->id);
    //SensorType t = sm.getType(this->id);

    //sempre if (t == MOTOR)
        s = new Motor(en);

    String ret = "";

    if (this->execMode == 1) {
        bool ok = tcpExecute(s);
        ret = ok ? "OK" : "ERROR";
    } else {
        int e = udpExecute(s);
        ret = String(e);
    }

    delete s;
    return ret;
}

char Command::getExecMode() const {
    return execMode;
}

char Command::getId() const {
    return id;
}

char Command::getAction() const {
    return action;
}

int Command::getRepeat() const {
    return repeat;
}

Command::Command(String command) {
    int i = 0;
    while(command.indexOf(':') > 0) {
        char c = command.substring(0, command.indexOf(':') - 1).charAt(0);
        if (i == 0) {
            execMode = c;
        } else if (i == 1) {
            id = c;
        } else if (i == 2) {
            action = c;
        } else if (i == 3) {
            //Last
            repeat = command.toInt();
        }

        command = command.substring(command.indexOf(':') + 1);
        i++;
    }
}

bool Command::tcpExecute(Sensor* sensor) const {
    int i;
    for (i = 0; i < this->repeat; i++)
        if(!sensor->doAction(this->action))
            return false;

    return true;
}

int Command::udpExecute(Sensor* sensor) const {
    int i;
    int e = 0;
    for (i = 0; i < this->repeat; i++)
        if(!sensor->doAction(this->action))
            e++;

    return e;
}

