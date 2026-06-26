@echo off
echo Building Proactive IT Agent...
cd "%~dp0\..\agent"
if not exist "build" (
    mkdir build
)
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows
if %ERRORLEVEL% NEQ 0 (
    echo CMake Configuration Failed!
    exit /b %ERRORLEVEL%
)
cmake --build build --config Debug
if %ERRORLEVEL% NEQ 0 (
    echo CMake Build Failed!
    exit /b %ERRORLEVEL%
)
echo Agent Build Successful!
