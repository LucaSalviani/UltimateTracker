@echo off
chcp 65001 >nul
echo Instalando Qt 6.8 (MinGW) en C:\Qt. Puede tardar entre 10 y 30 minutos...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0instalar-qt.ps1"
echo.
pause
