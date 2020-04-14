cmake -E make_directory "build" && cmake -E chdir "build" cmake -G "Visual Studio 16 2019" -A "x64" -DCMAKE_INSTALL_PREFIX="vortice-sdk-windows" ..
