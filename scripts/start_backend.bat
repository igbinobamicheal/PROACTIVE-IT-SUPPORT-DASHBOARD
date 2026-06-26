@echo off
echo Starting Proactive IT Dashboard Backend...
cd "%~dp0\..\backend"
if exist "build\Debug\proactive_it_dashboard.exe" (
    cd build\Debug
    proactive_it_dashboard.exe
) else (
    echo Backend binary not found! Please build first by running build_backend.bat.
)
