mkdir build\
meson setup build --buildtype=debug
cd build
ninja
meson install
cd ..