# Disabilita l'installazione automatica del manifest vcpkg per evitare fallimenti automatici
if (NOT DEFINED VCPKG_MANIFEST_INSTALL)
    set(VCPKG_MANIFEST_INSTALL OFF CACHE BOOL "Disable automatic vcpkg manifest install to avoid failures")
endif ()

cmake_minimum_required(VERSION 3.20)
project(3DP_Driver_Core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Piattaforma (ARDUINO, ESP32, NATIVE)
if (NOT DEFINED PLATFORM)
    set(PLATFORM "ARDUINO" CACHE STRING "Target platform: ARDUINO, ESP32, NATIVE")
endif ()
string(TOUPPER "${PLATFORM}" PLATFORM)
message(STATUS "Building for platform: ${PLATFORM}")

# Raccoglie ricorsivamente tutti i sorgenti nel progetto (evita di elencarli a mano)
file(GLOB_RECURSE PROJECT_SOURCES
        "${CMAKE_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_SOURCE_DIR}/src/*.c"
)

# Aggiunge il file .ino se presente (utile per build Arduino IDE style)
set(INO_FILE "${CMAKE_SOURCE_DIR}/3D-Printer-Firmware.ino")
if (EXISTS ${INO_FILE})
    list(APPEND PROJECT_SOURCES ${INO_FILE})
endif ()

# HAL specifici per piattaforma
set(HAL_SOURCES "")
set(HAL_INCLUDE "")

if (PLATFORM STREQUAL "ARDUINO")
    list(APPEND HAL_SOURCES "${CMAKE_SOURCE_DIR}/src/hal/arduino/ArduinoHAL.cpp")
    set(HAL_INCLUDE "${CMAKE_SOURCE_DIR}/src/include/hal/arduino")
    # Definizioni comuni per Arduino (non aggiungere flag del compilatore non compatibili con host)
    add_compile_definitions(ARDUINO=10805 F_CPU=16000000L ARDUINO_AVR_UNO __AVR__)

elseif (PLATFORM STREQUAL "ESP32")
    # ESP32 HAL source file - MUST be moved to correct location!
    # Current location: src/hal/arduino/ESP32HAL.cpp (WRONG)
    # Expected location: src/hal/esp32/ESP32HAL.cpp (CORRECT)

    # Check if ESP32HAL.cpp exists in correct location
    if (EXISTS "${CMAKE_SOURCE_DIR}/src/hal/esp32/ESP32HAL.cpp")
        list(APPEND HAL_SOURCES "${CMAKE_SOURCE_DIR}/src/hal/esp32/ESP32HAL.cpp")
        message(STATUS "ESP32HAL.cpp found in correct location")
    elseif (EXISTS "${CMAKE_SOURCE_DIR}/src/hal/arduino/ESP32HAL.cpp")
        list(APPEND HAL_SOURCES "${CMAKE_SOURCE_DIR}/src/hal/arduino/ESP32HAL.cpp")
        message(WARNING "ESP32HAL.cpp found in WRONG location! Please move from src/hal/arduino/ to src/hal/esp32/")
    else ()
        message(FATAL_ERROR "ESP32HAL.cpp not found! Expected at src/hal/esp32/ESP32HAL.cpp")
    endif ()

    set(HAL_INCLUDE "${CMAKE_SOURCE_DIR}/src/include/hal/esp32")

    # ESP32-specific definitions
    add_compile_definitions(
            PLATFORM_ESP32
            ESP32
            ARDUINO=10819
            ARDUINO_ESP32_DEV
            ARDUINO_ARCH_ESP32
            CONFIG_FREERTOS_HZ=1000
            ESP32_HAL_ENABLED
    )

    # ESP32-specific compiler flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -fno-rtti")

elseif (PLATFORM STREQUAL "NATIVE")
    list(APPEND HAL_SOURCES "${CMAKE_SOURCE_DIR}/src/hal/native/NativeHAL.cpp")
    set(HAL_INCLUDE "${CMAKE_SOURCE_DIR}/src/include/hal/native")
    find_package(Threads REQUIRED)

else ()
    message(FATAL_ERROR "Unknown platform: ${PLATFORM})")
endif ()

# Unisce i sorgenti HAL raccolti
list(APPEND PROJECT_SOURCES ${HAL_SOURCES})

# Crea la libreria del firmware (tutti i sorgenti vengono aggiunti automaticamente)
add_library(3dp_firmware_lib STATIC ${PROJECT_SOURCES})

# Include directories generali
target_include_directories(3dp_firmware_lib PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${HAL_INCLUDE}
)

# Link specifici per piattaforma
if (PLATFORM STREQUAL "NATIVE")
    add_executable(3dp_firmware_test
            ${CMAKE_SOURCE_DIR}/test/main.cpp
    )

    target_link_libraries(3dp_firmware_test
            3dp_firmware_lib
            Threads::Threads
    )

    # Enable testing
    enable_testing()
    add_test(NAME BasicTest COMMAND 3dp_firmware_test)

elseif (PLATFORM STREQUAL "ESP32")
    # ESP32-specific configuration
    if (EXISTS ${INO_FILE})
        add_executable(3dp_firmware ${INO_FILE})
    else ()
        # fallback: crea un eseguibile vuoto che linka la libreria per consentire l'IDE di riconoscerla
        add_executable(3dp_firmware EXCLUDE_FROM_ALL dummy.cpp)
    endif ()

    target_link_libraries(3dp_firmware
            3dp_firmware_lib
    )

    # ESP32 specific linking flags
    target_link_options(3dp_firmware PRIVATE
            -Wl,--gc-sections
            -Wl,-Map,${CMAKE_BINARY_DIR}/3dp_firmware.map
    )

else ()
    # Per piattaforme embedded: crea un 'executable' che punta allo sketch .ino (se presente)
    if (EXISTS ${INO_FILE})
        add_executable(3dp_firmware ${INO_FILE})
    else ()
        # fallback: crea un eseguibile vuoto che linka la libreria per consentire l'IDE di riconoscerla
        add_executable(3dp_firmware EXCLUDE_FROM_ALL dummy.cpp)
    endif ()

    target_link_libraries(3dp_firmware
            3dp_firmware_lib
    )
endif ()

# Esporta le compile commands per gli IDE
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Install rules
install(TARGETS 3dp_firmware_lib
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/include/
        DESTINATION include/3dp_firmware
        FILES_MATCHING PATTERN "*.hpp"
)

# Platform-specific install rules
if (PLATFORM STREQUAL "ESP32")
    install(FILES ${CMAKE_BINARY_DIR}/3dp_firmware.map
            DESTINATION bin
            OPTIONAL)
endif ()