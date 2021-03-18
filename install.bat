mkdir build\
meson setup build --buildtype=release
cd build
ninja
meson install
cd ..