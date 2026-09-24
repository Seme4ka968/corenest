@echo off
cd /d "%~dp0"
build\corenest.exe %*
if errorlevel 1 pause
