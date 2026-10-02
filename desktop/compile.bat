@echo off
echo ====================================================
echo             Compiling AirBridge for Windows        
echo ====================================================

where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo Found g++. Compiling with GCC...
    g++ -O3 -std=c++17 main.cpp -o airbridge.exe -lws2_32 -lshell32
    if %ERRORLEVEL% equ 0 (
        echo Compilation successful! Created airbridge.exe
        exit /b 0
    ) else (
        echo Compilation failed with g++.
    )
)

where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo Found clang++. Compiling with Clang...
    clang++ -O3 -std=c++17 main.cpp -o airbridge.exe -lws2_32 -lshell32
    if %ERRORLEVEL% equ 0 (
        echo Compilation successful! Created airbridge.exe
        exit /b 0
    ) else (
        echo Compilation failed with clang++.
    )
)

echo.
echo [ERROR] No suitable compiler found in PATH (g++ or clang++).
echo Make sure you have MinGW or LLVM installed and added to your environment PATH.
exit /b 1
