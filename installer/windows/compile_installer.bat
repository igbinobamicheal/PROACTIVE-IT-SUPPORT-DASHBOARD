@echo off
setlocal enabledelayedexpansion

echo === Proactive IT Agent Installer Compiler ===

:: Check if ISCC is in PATH
where ISCC.exe >nul 2>nul
if !errorlevel! equ 0 (
    set "ISCC=ISCC.exe"
    goto :compile
)

:: Check standard installation path
set "ISCC_PATH=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if exist "!ISCC_PATH!" (
    set "ISCC=!ISCC_PATH!"
    goto :compile
)

echo [ERROR] Inno Setup compiler (ISCC.exe) was not found in your PATH or in "C:\Program Files (x86)\Inno Setup 6\".
echo Please install Inno Setup 6 (https://jrsoftware.org/isdownload.php) and try again.
exit /b 1

:compile
echo [INFO] Found Inno Setup Compiler: "!ISCC!"
echo [INFO] Compiling installer.iss...
"!ISCC!" installer.iss
if !errorlevel! equ 0 (
    echo [SUCCESS] Installer compiled successfully: ProactiveITAgentSetup.exe
    exit /b 0
) else (
    echo [ERROR] Compilation failed.
    exit /b 1
)
