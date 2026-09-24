@echo off
cd /d "%~dp0"
build\corenest.exe --scale=3 --pos=50,50 cores\gambatte_libretro.dll roms\game.gb
if errorlevel 1 pause
