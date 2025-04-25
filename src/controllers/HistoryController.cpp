#include <Arduino.h>
#include "./../include/controllers/HistoryController.hpp"
#include "./../include/SerialCommandReceiver.hpp"
#include "./../include/CommandHistory.hpp"

namespace HistoryController {

    void handle(int code, const char *params) {
        switch (code) {

            case 10:
                Serial.println(F("=== HISTORY BUFFER ==="));
                for (uint16_t i = 0; i < 256; ++i) {
                    const char *cmd = CommandHistory::get(i);
                    if (cmd) {
                        Serial.print(F("N"));
                        Serial.print(i);
                        Serial.print(F(": "));
                        Serial.println(cmd);
                    }
                }
                break;

            case 20: {
                const char *p = strchr(params, 'N');
                if (p) {
                    uint16_t n = atoi(p + 1);
                    const char *cmd = CommandHistory::get(n);
                    if (cmd) {
                        Serial.print(F("RESEND N"));
                        Serial.println(n);
                        Serial.println(cmd);
                    } else {
                        Serial.print(F("RESEND FAILED N"));
                        Serial.println(n);
                    }
                }
                break;
            }

            case 30:
                CommandHistory::clear();
                Serial.println(F("History cleared"));
                break;

            case 40:
                Serial.print(F("Last command N"));
                Serial.println(SerialCommandReceiver::lastCommandNumber);
                break;

            default:
                Serial.println(F("Unknown history command"));
        }
    }

}
