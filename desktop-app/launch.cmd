@echo off
setlocal
cd /d "%~dp0"
set "JOY_VENV=%LOCALAPPDATA%\JoystickEditor\venv"
if not exist "%JOY_VENV%\Scripts\python.exe" (
    py -3 -m venv "%JOY_VENV%" || goto failed
)
"%JOY_VENV%\Scripts\python.exe" -c "import serial, tkinter" >nul 2>&1
if errorlevel 1 (
    "%JOY_VENV%\Scripts\python.exe" -m pip install -r requirements.txt || goto failed
)
"%JOY_VENV%\Scripts\python.exe" app.py
if errorlevel 1 goto failed
exit /b 0
:failed
echo Joystick-Editor konnte nicht gestartet werden.
pause
exit /b 1
