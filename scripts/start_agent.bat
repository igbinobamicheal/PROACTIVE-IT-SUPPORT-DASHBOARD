@echo off
echo Starting Proactive IT Agent...
cd "%~dp0\..\agent"
if exist "build\Debug\proactive_it_agent.exe" (
    cd build\Debug
    proactive_it_agent.exe
) else (
    echo Agent binary not found! Please build first by running build_agent.bat.
)
