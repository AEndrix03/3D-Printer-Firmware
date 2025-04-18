//
// Created by redeg on 12/08/2024.
//

#ifndef INC_3D_PRINTER_FIRMWARE_SENSORSCONFIGURATION_H
#define INC_3D_PRINTER_FIRMWARE_SENSORSCONFIGURATION_H

namespace config {
    //Axis MM PER STEP
    const double X_MM_PER_STEP = 0.433;
    const double Y_MM_PER_STEP = 0.410;
    const double Z_MM_PER_STEP = 0.013;
    const double A_MM_PER_STEP = 0.287;

    //Axis Max Speed
    const int X_MAX_SPEED = 200;
    const int Y_MAX_SPEED = 200;
    const int Z_MAX_SPEED = 100;
    const int A_MAX_SPEED = 200;

    //Bed Size (in mm)
    const int X_BED_SIZE = 100;
    const int Y_BED_SIZE = 150;

    //Offsets (in mm)
    const int X_OFFSET_MIN = 10;
    const int Y_OFFSET_MIN = 10;
    const int Z_OFFSET_MIN = 10;

    const int X_OFFSET_MAX = 90;
    const int Y_OFFSET_MAX = 140;
    const int Z_OFFSET_MAX = 200;

    const bool X_INVERT_DIRECTION = false;
    const bool Y_INVERT_DIRECTION = false;
    const bool Z_INVERT_DIRECTION = true;
    const bool A_INVERT_DIRECTION = true;

    const bool ENABLE_ERRORS_LED_LOG = false;
    const int ERROR_DELAY_MS = 500;

    const double TEMP_SENSOR_0_RES_0 = 100000;
    const double TEMP_SENSOR_0_TEMP_0 = 298.15;
    const double TEMP_SENSOR_0_BETA = 3950;
    const double TEMP_SENSOR_0_SER_RES = 16370;
};

#endif //INC_3D_PRINTER_FIRMWARE_SENSORSCONFIGURATION_H
