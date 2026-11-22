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

}
