@echo off
cmake -S "./../" -B "vs2022_arm64" -G "Visual Studio 17 2022" -A ARM64 -DCMAKE_INSTALL_PREFIX:String="SDK" %*
echo Open vs2022_arm64\vgpu.sln to build the project.