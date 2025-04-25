#include "./include/CommandHistory.hpp"
#include <string.h>

namespace {

    struct CommandSlot {
        uint16_t number;
        char text[64];
    };

    CommandSlot buffer[CommandHistory::BUFFER_SIZE];
    uint8_t head = 0;

}

namespace CommandHistory {

    void init() {
        for (uint8_t i = 0; i < BUFFER_SIZE; ++i) {
            buffer[i].number = 0xFFFF;
            buffer[i].text[0] = '\0';
        }
    }

    void store(uint16_t number, const char *text) {
        buffer[head].number = number;
        strncpy(buffer[head].text, text, sizeof(buffer[head].text) - 1);
        buffer[head].text[sizeof(buffer[head].text) - 1] = '\0';
        head = (head + 1) % BUFFER_SIZE;
    }

    const char *get(uint16_t number) {
        for (uint8_t i = 0; i < BUFFER_SIZE; ++i) {
            if (buffer[i].number == number) {
                return buffer[i].text;
            }
        }
        return nullptr;
    }

    void clear() {
        for (uint8_t i = 0; i < BUFFER_SIZE; ++i) {
            buffer[i].number = 0xFFFF;
            buffer[i].text[0] = '\0';
        }
    }

}
