@echo off
cd /d "%~dp0"
build\corenest.exe --scale=3 --pos=700,50 cores\mgba_libretro.dll roms\game1.gba
if errorlevel 1 pause
