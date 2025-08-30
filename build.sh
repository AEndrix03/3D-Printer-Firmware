#!/bin/bash
# build.sh - Multi-platform build script for 3D Printer Firmware

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default values
PLATFORM="ARDUINO"
BUILD_TYPE="Release"
VERBOSE=0

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --platform)
            PLATFORM="$2"
            shift 2
            ;;
        --build-type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        --verbose)
            VERBOSE=1
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --platform [ARDUINO|ESP32|NATIVE]  Target platform (default: ARDUINO)"
            echo "  --build-type [Debug|Release]       Build type (default: Release)"
            echo "  --verbose                          Enable verbose output"
            echo "  --help                             Show this help message"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

echo -e "${GREEN}Building 3D Printer Firmware${NC}"
echo "Platform: $PLATFORM"
echo "Build Type: $BUILD_TYPE"

# Check for required tools
check_tool() {
    if ! command -v $1 &> /dev/null; then
        echo -e "${RED}Error: $1 is not installed${NC}"
        exit 1
    fi
}

# Create build directory
BUILD_DIR="build-${PLATFORM,,}-${BUILD_TYPE,,}"
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# Platform-specific build
case $PLATFORM in
    ARDUINO)
        echo -e "${YELLOW}Building for Arduino...${NC}"
        check_tool arduino-cli

        # Using arduino-cli
        if [ ! -f "../arduino-cli.yaml" ]; then
            arduino-cli config init
        fi

        arduino-cli compile --fqbn arduino:avr:uno \
            --build-property "compiler.cpp.extra_flags=-DPLATFORM_ARDUINO" \
            --output-dir . \
            ..

        echo -e "${GREEN}Arduino build complete!${NC}"
        echo "Upload with: arduino-cli upload -p /dev/ttyUSB0 --fqbn arduino:avr:uno --input-dir ."
        ;;

    ESP32)
        echo -e "${YELLOW}Building for ESP32...${NC}"
        check_tool platformio

        # Using PlatformIO
        cd ..
        platformio run -e esp32

        echo -e "${GREEN}ESP32 build complete!${NC}"
        echo "Upload with: platformio run -e esp32 --target upload"
        ;;

    NATIVE)
        echo -e "${YELLOW}Building native version for testing...${NC}"
        check_tool cmake
        check_tool make

        # Configure with CMake
        cmake .. \
            -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
            -DPLATFORM=NATIVE \
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

        # Build
        if [ $VERBOSE -eq 1 ]; then
            make VERBOSE=1 -j$(nproc)
        else
            make -j$(nproc)
        fi

        echo -e "${GREEN}Native build complete!${NC}"
        echo "Run tests with: ctest --verbose"
        ;;

    *)
        echo -e "${RED}Unknown platform: $PLATFORM${NC}"
        exit 1
        ;;
esac

echo -e "${GREEN}Build successful!${NC}"