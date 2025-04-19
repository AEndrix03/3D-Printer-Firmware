#include "./include/StateMachine.hpp"

namespace {
    MachineState current = MachineState::Idle;
}

namespace StateMachine {

    void setState(MachineState newState) {
        current = newState;
    }

    MachineState getState() {
        return current;
    }

    const char *getStateName() {
        switch (current) {
            case MachineState::Idle:
                return "Idle";
            case MachineState::Homing:
                return "Homing";
            case MachineState::Printing:
                return "Printing";
            case MachineState::Paused:
                return "Paused";
            case MachineState::Error:
                return "Error";
            default:
                return "Unknown";
        }
    }

}
