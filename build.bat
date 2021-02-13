del /s /q build\*
rmdir /s /q build\
mkdir build\
meson setup build --buildtype=release -Dcompiler=disabled
cd build
ninja
cd ..