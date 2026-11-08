@echo off
setlocal EnableExtensions EnableDelayedExpansion
title Joystick firmware - local QMK build

set "SOURCE=%~dp0"
set "QMK_ROOT="

REM Look for the actual QMK firmware checkout.
if defined QMK_HOME if exist "%QMK_HOME%\quantum\quantum.c" set "QMK_ROOT=%QMK_HOME%"
if not defined QMK_ROOT (
    for %%D in (
        "%SOURCE%qmk_firmware"
        "%SOURCE%..\qmk_firmware"
        "%USERPROFILE%\qmk_firmware"
        "%USERPROFILE%\Documents\qmk_firmware"
        "C:\QMK_MSYS\home\%USERNAME%\qmk_firmware"
    ) do (
        if not defined QMK_ROOT if exist "%%~fD\quantum\quantum.c" set "QMK_ROOT=%%~fD"
    )
)

if not defined QMK_ROOT (
    echo ERROR: qmk_firmware folder not found.
    echo Set QMK_HOME to the root of your QMK checkout and run this BAT again.
    goto :failed
)

set "BUILD_WITH_MSYS=0"
where qmk >nul 2>&1
if errorlevel 1 (
    REM QMK MSYS fallback allows a normal BAT double-click.
    set "BUILD_WITH_MSYS=1"
    if not exist "C:\QMK_MSYS\usr\bin\bash.exe" goto :missing_cli
    if not exist "C:\QMK_MSYS\usr\bin\cygpath.exe" goto :missing_cli
)

echo.
echo QMK source: %QMK_ROOT%
echo Joystick:   %SOURCE%
echo.

set "TARGET=%QMK_ROOT%\keyboards\joystick_pad"
if not exist "%TARGET%\keymaps\default" mkdir "%TARGET%\keymaps\default" || goto :failed

REM Stage only QMK files. The legacy ESP32-C3 Arduino sketch is not included.
for %%F in (config.h keyboard.json rules.mk mcuconf.h) do (
    copy /Y "%SOURCE%%%F" "%TARGET%\%%F" >nul
    if errorlevel 1 goto :failed
)
copy /Y "%SOURCE%keymaps\default\keymap.c" "%TARGET%\keymaps\default\keymap.c" >nul
if errorlevel 1 goto :failed

REM Delete obsolete I2C config from previously staged versions.
if exist "%TARGET%\halconf.h" del /Q "%TARGET%\halconf.h"

echo Compiling RP2040 QMK firmware...
if "!BUILD_WITH_MSYS!"=="1" (
    REM Convert Windows path to MSYS /c/... format before running QMK.
    for /f "delims=" %%P in ('C:\QMK_MSYS\usr\bin\cygpath.exe -u "%QMK_ROOT%"') do set "QMK_POSIX=%%P"
    if not defined QMK_POSIX goto :failed
    "C:\QMK_MSYS\usr\bin\bash.exe" -lc "cd '!QMK_POSIX!' && qmk compile -kb joystick_pad -km default"
    set "COMPILE_RESULT=!ERRORLEVEL!"
) else (
    pushd "%QMK_ROOT%"
    call qmk compile -kb joystick_pad -km default
    set "COMPILE_RESULT=!ERRORLEVEL!"
    popd
)
if not "!COMPILE_RESULT!"=="0" goto :failed

if not exist "%SOURCE%artifacts" mkdir "%SOURCE%artifacts"
set "FOUND=0"
for %%F in (
    "%QMK_ROOT%\joystick_pad_default.uf2"
    "%QMK_ROOT%\.build\joystick_pad_default.uf2"
) do (
    if exist "%%~fF" (
        copy /Y "%%~fF" "%SOURCE%artifacts\%%~nxF" >nul
        if errorlevel 1 goto :failed
        set "FOUND=1"
    )
)

if "!FOUND!"=="0" (
    echo Build reported success but no joystick_pad_default.uf2 was found.
    goto :failed
)

echo.
echo SUCCESS: firmware in "%SOURCE%artifacts"
echo Transfer the UF2 to the RP2040 bootloader drive.
pause
exit /b 0

:missing_cli
echo ERROR: No QMK CLI was found on PATH and QMK MSYS was not found at C:\QMK_MSYS.
echo Install QMK MSYS, or run this BAT with QMK available on PATH.
goto :failed

:failed
echo.
echo Local build did not complete.
pause
exit /b 1
