@echo off
cd /d "%~dp0"
if not exist PixelCraft.exe call build.bat
if errorlevel 1 exit /b 1
PixelCraft.exe
pause
