//
// Created by redeg on 12/08/2024.
//

#ifndef INC_3D_PRINTER_FIRMWARE_THROWABLE_H
#define INC_3D_PRINTER_FIRMWARE_THROWABLE_H

class Throwable {
    virtual void handleException() = 0;
};

#endif //INC_3D_PRINTER_FIRMWARE_THROWABLE_H
