#!/bin/bash
echo "===================================================="
echo "            Compiling AirBridge for Linux           "
echo "===================================================="

AIRBRIDGE_VERSION="$(tr -d '\r\n' < ../VERSION 2>/dev/null || printf 'dev')"

if command -v g++ >/dev/null 2>&1; then
    echo "Found g++. Compiling..."
    g++ -O3 -std=c++17 "-DAIRBRIDGE_VERSION=\"${AIRBRIDGE_VERSION}\"" main.cpp -o airbridge -pthread
    if [ $? -eq 0 ]; then
        echo "Compilation successful! Created airbridge"
        exit 0
    else
        echo "Compilation failed with g++."
        exit 1
    fi
elif command -v clang++ >/dev/null 2>&1; then
    echo "Found clang++. Compiling..."
    clang++ -O3 -std=c++17 "-DAIRBRIDGE_VERSION=\"${AIRBRIDGE_VERSION}\"" main.cpp -o airbridge -pthread
    if [ $? -eq 0 ]; then
        echo "Compilation successful! Created airbridge"
        exit 0
    else
        echo "Compilation failed with clang++."
        exit 1
    fi
else
    echo "[ERROR] No compiler (g++ or clang++) found on system."
    echo "Install g++: sudo apt install build-essential"
    exit 1
fi
