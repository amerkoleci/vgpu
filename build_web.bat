@echo off

if not exist build\web (
    mkdir build\web
)
cd build\web
cmake ..\..\ -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=%EMSDK%/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build .
cd ..\..\
pause