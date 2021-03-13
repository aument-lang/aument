mkdir build\
meson setup build --buildtype=debug -Db_sanitize=address
cd build
ninja
cd ..