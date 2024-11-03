//
// Created by redeg on 12/08/2024.
//

#ifndef INC_3D_PRINTER_FIRMWARE_SENSORPINOUT_H
#define INC_3D_PRINTER_FIRMWARE_SENSORPINOUT_H

#include <pins_arduino.h>

namespace pins {
    //Temp sensor
    const int TEMP_SENSOR_0 = A4;

    //Movement of the Stepper Motors
    const int X_STEP = 2;
    const int Y_STEP = 3;
    const int Z_STEP = 4;
    const int A_STEP = 12;

    //Direction of the Stepper Motors
    const int X_DIR = 5;
    const int Y_DIR = 6;
    const int Z_DIR = 7;
    const int A_DIR = 13;

    //End Stops of the Axis
    const int X_ENDSTOP = 9;
    const int Y_ENDSTOP = 10;
    const int Z_ENDSTOP = 11;

    //Error
    const int ERROR_LED = 1;

    //Temperature
    const int TEMP = A4;
}

#endif //INC_3D_PRINTER_FIRMWARE_SENSORPINOUT_H
