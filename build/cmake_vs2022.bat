@echo off
cmake -B "vs2022_x64" -S "./../" -G "Visual Studio 17 2022" -A x64 -DCMAKE_INSTALL_PREFIX:String="SDK" %*
echo Open vs2022_x64\vgpu.sln to build the project.