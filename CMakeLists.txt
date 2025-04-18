cmake_minimum_required(VERSION 3.20)
project(3D-Printer-Firmware CXX)

set(ARDUINO_CLI "arduino-cli")
set(BOARD_FQBN "arduino:avr:uno")
set(PORT "COM3")
set(CMAKE_CXX_STANDARD 14)

include_directories(
        "C:/Users/redeg/AppData/Local/Arduino15/packages/arduino/hardware/avr/1.8.6/cores/arduino"
        "C:/Users/redeg/AppData/Local/Arduino15/packages/arduino/hardware/avr/1.8.6/variants/standard"
)

add_executable(3D-Printer-Firmware ./src/Application.cpp)

add_custom_target(compile
    COMMAND ${ARDUINO_CLI} compile --fqbn ${BOARD_FQBN} ${CMAKE_SOURCE_DIR}
    COMMENT "Compilazione del progetto Arduino"
)

add_custom_target(upload
    COMMAND ${ARDUINO_CLI} upload -p ${PORT} --fqbn ${BOARD_FQBN} ${CMAKE_SOURCE_DIR}
    COMMENT "Upload del progetto sulla board"
    DEPENDS compile
)
