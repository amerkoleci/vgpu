@echo off

cmake cmake . -G "Visual Studio 17 2022" -A ARM64 -Bbuild_arm64 -DCMAKE_INSTALL_PREFIX="Sdk"
pause
