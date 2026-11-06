@echo off
cd /d "%~dp0"
g++ -std=c++17 -O2 -Wall -Wextra PixelCraft.cpp -o PixelCraft.exe -lfreeglut -lopengl32 -lglu32
if errorlevel 1 exit /b 1
echo Build complete. Run PixelCraft.exe from this folder.
