@echo off

cmake -B ".\build\uwp" -S "." -G "Visual Studio 17 2022" -A x64 -DCMAKE_SYSTEM_NAME:String=WindowsStore -DCMAKE_SYSTEM_VERSION:String="10.0" -DCMAKE_INSTALL_PREFIX="Sdk"
pause
