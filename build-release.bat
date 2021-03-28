mkdir build\
meson setup build --buildtype=release || exit 1
cd build
ninja || exit 1
cd ..