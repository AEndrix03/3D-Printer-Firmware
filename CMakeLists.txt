cmake_minimum_required(VERSION 3.20)
project(3D-Printer-Firmware CXX)
set(CMAKE_CXX_STANDARD 14)

# Path della toolchain installata
set(ARDUINO_CORE_PATH "C:/Users/Andrea/AppData/Local/Arduino15/packages/arduino/hardware/avr/1.8.6")

include_directories(
        ${ARDUINO_CORE_PATH}/cores/arduino
        ${ARDUINO_CORE_PATH}/variants/standard
        "C:/Users/Andrea/AppData/Local/Arduino15/packages/arduino/tools/avr-gcc/7.3.0-atmel3.6.1-arduino7/avr/include"
)

add_definitions(
        -DARDUINO=10805
        -DF_CPU=16000000L
        -DARDUINO_AVR_UNO
        -D__AVR__
        -mmcu=atmega328p
)

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
        "src/*.cpp"
        "include/**/*.hpp"
)

add_executable(3D-Printer-Firmware ${SRC_FILES}
        src/include/motion/MotionConfig.hpp
        src/motion/MotionPlanner.cpp
        src/include/motion/MotionPlanner.hpp)
