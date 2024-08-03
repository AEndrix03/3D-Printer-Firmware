//
// Created by redeg on 24/06/2024.
//

#ifndef INC_3D_PRINTER_FIRMWARE_SERIALLISTENER_H
#define INC_3D_PRINTER_FIRMWARE_SERIALLISTENER_H

template<typename T>
class SerialListener {
protected:
    virtual void doAction(T action) = 0;
};

#endif //INC_3D_PRINTER_FIRMWARE_SERIALLISTENER_H
