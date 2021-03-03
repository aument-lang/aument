del /s /q build\*
rmdir /s /q build\
mkdir build\
meson setup build --buildtype=release
cd build
ninja
cd ..