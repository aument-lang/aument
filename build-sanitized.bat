mkdir build\
meson setup build --buildtype=debug -Db_sanitize=address || exit 1
cd build
ninja || exit 1
cd ..