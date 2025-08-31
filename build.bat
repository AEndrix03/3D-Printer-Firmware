@echo off
setlocal enabledelayedexpansion

REM build.bat - Multi-platform build script for 3D Printer Firmware (Windows)

REM Colors for output (Windows 10+ with ANSI support)
set "RED=[91m"
set "GREEN=[92m"
set "YELLOW=[93m"
set "NC=[0m"

REM Default values
set "PLATFORM=ARDUINO"
set "BUILD_TYPE=Release"
set "VERBOSE=0"

REM Parse arguments
:parse_args
if "%~1"=="" goto end_parse
if "%~1"=="--platform" (
    set "PLATFORM=%~2"
    shift
    shift
    goto parse_args
)
if "%~1"=="--build-type" (
    set "BUILD_TYPE=%~2"
    shift
    shift
    goto parse_args
)
if "%~1"=="--verbose" (
    set "VERBOSE=1"
    shift
    goto parse_args
)
if "%~1"=="--help" (
    echo Usage: %~nx0 [options]
    echo Options:
    echo   --platform [ARDUINO^|ESP32^|NATIVE]  Target platform (default: ARDUINO^)
    echo   --build-type [Debug^|Release]       Build type (default: Release^)
    echo   --verbose                          Enable verbose output
    echo   --help                             Show this help message
    exit /b 0
)
echo %RED%Unknown option: %~1%NC%
exit /b 1

:end_parse

echo %GREEN%Building 3D Printer Firmware%NC%
echo Platform: %PLATFORM%
echo Build Type: %BUILD_TYPE%

REM Create build directory
set "BUILD_DIR=build-%PLATFORM%-%BUILD_TYPE%"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

REM Platform-specific build
if /i "%PLATFORM%"=="ARDUINO" goto build_arduino
if /i "%PLATFORM%"=="ESP32" goto build_esp32
if /i "%PLATFORM%"=="NATIVE" goto build_native

echo %RED%Unknown platform: %PLATFORM%%NC%
exit /b 1

:build_arduino
echo %YELLOW%Building for Arduino...%NC%

REM Check for arduino-cli
where arduino-cli >nul 2>&1
if errorlevel 1 (
    echo %RED%Error: arduino-cli is not installed or not in PATH%NC%
    echo Please install Arduino CLI from: https://arduino.github.io/arduino-cli/
    exit /b 1
)

REM Initialize config if needed
if not exist "..\arduino-cli.yaml" (
    echo Initializing Arduino CLI config...
    arduino-cli config init
)

REM Update index
echo Updating Arduino package index...
arduino-cli core update-index

REM Install AVR core if needed
arduino-cli core list | findstr "arduino:avr" >nul
if errorlevel 1 (
    echo Installing Arduino AVR core...
    arduino-cli core install arduino:avr
)

REM Compile
echo Compiling Arduino project...
arduino-cli compile --fqbn arduino:avr:uno ^
    --build-property "compiler.cpp.extra_flags=-DPLATFORM_ARDUINO" ^
    --output-dir . ^
    ..

if errorlevel 1 (
    echo %RED%Arduino build failed!%NC%
    exit /b 1
)

echo %GREEN%Arduino build complete!%NC%
echo Upload with: arduino-cli upload -p COM3 --fqbn arduino:avr:uno --input-dir .
goto end

:build_esp32
echo %YELLOW%Building for ESP32...%NC%

REM Check for platformio
where pio >nul 2>&1
if errorlevel 1 (
    echo %RED%Error: PlatformIO is not installed or not in PATH%NC%
    echo Please install PlatformIO: pip install platformio
    exit /b 1
)

REM Build with PlatformIO
cd /d ..
pio run -e esp32

if errorlevel 1 (
    echo %RED%ESP32 build failed!%NC%
    exit /b 1
)

echo %GREEN%ESP32 build complete!%NC%
echo Upload with: pio run -e esp32 --target upload
goto end

:build_native
echo %YELLOW%Building native version for testing...%NC%

REM Check for cmake
where cmake >nul 2>&1
if errorlevel 1 (
    echo %RED%Error: cmake is not installed or not in PATH%NC%
    exit /b 1
)

REM Check for build system
where ninja >nul 2>&1
if not errorlevel 1 (
    set "GENERATOR=-G Ninja"
) else (
    where msbuild >nul 2>&1
    if not errorlevel 1 (
        set "GENERATOR=-G "Visual Studio 16 2019""
    ) else (
        echo %RED%Error: No suitable build system found (Ninja or Visual Studio^)%NC%
        exit /b 1
    )
)

REM Configure with CMake
echo Configuring with CMake...
cmake .. ^
    %GENERATOR% ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DPLATFORM=NATIVE ^
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

if errorlevel 1 (
    echo %RED%CMake configuration failed!%NC%
    exit /b 1
)

REM Build
echo Building...
if "%VERBOSE%"=="1" (
    cmake --build . --config %BUILD_TYPE% --verbose
) else (
    cmake --build . --config %BUILD_TYPE%
)

if errorlevel 1 (
    echo %RED%Native build failed!%NC%
    exit /b 1
)

echo %GREEN%Native build complete!%NC%
echo Run tests with: ctest --verbose
goto end

:end
cd /d ..
echo %GREEN%Build successful!%NC%
endlocal