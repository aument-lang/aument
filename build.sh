#!/bin/bash
# This source file is part of the aulang project
# Copyright (c) 2021 the aulang contributors
#
# Licensed under Apache License v2.0 with Runtime Library Exception
# See LICENSE.txt for license information

MESONARG=""
CCFLAGS=""

for i in "$@"
do
case $i in
    -D*)
        CCFLAGS="$CCFLAGS $i"
        shift
    ;;
    -f|--force)
        rm -rf build
        shift
    ;;
    -r|--reconfigure)
        MESONARG="$MESONARG --wipe"
        shift
    ;;
    --sanitize)
        MESONARG="$MESONARG -Db_sanitize=address"
        shift
    ;;
    *)
        MESONARG="$MESONARG --buildtype=$i"
        shift
    ;;
esac
done

echo "MESONARG: $MESONARG"
echo "CCFLAGS: $CCFLAGS"

meson setup build $MESONARG -Dc_args="$CCFLAGS"
cd build
ninja