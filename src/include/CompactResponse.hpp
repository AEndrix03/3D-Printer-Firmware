//
// Created by redeg on 02/09/2025.
//

#pragma once

#include <Arduino.h>

namespace CompactResponse {
    enum ResponseCode {
        OK = 0,
        CHECKSUM_ERROR,
        BUFFER_OVERFLOW_ERROR,
        DUPLICATE_COMMAND_ERROR,
        RESEND_COMMAND_ERROR,
        INVALID_CATEGORY_ERROR,
        BLOCKED_MOTION_ERROR,
        BLOCKED_TEMP_ERROR,
        CANCELLED_ERROR,
        NO_ERROR
    };

    void send(ResponseCode code, uint32_t cmdNumber);

    void sendNoCommand(ResponseCode code);

    void sendData(const char *dataType, const char *payload);

    void sendCriticalData(const char *dataType, const char *payload, uint8_t maxRetries = 3);

}