mkdir build\
meson setup build --buildtype=debug || exit 1
cd build
ninja || exit 1
cd ..