#!/bin/bash
# This source file is part of the aument project
# Copyright (c) 2021 the aument contributors
#
# Licensed under Apache License v2.0 with Runtime Library Exception
# See LICENSE.txt for license information

buildtype=""

for i in "$@"
do
case $i in
    -Wc,D*)
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
    --cross-win64)
    	MESONARG="$MESONARG --cross-file=cross/win64.txt"
    	shift
    ;;
    *)
        if [[ "$buildtype" = "" ]]; then
        	buildtype="$i"
        else
        	MESONARG="$MESONARG $i"
        fi
        shift
    ;;
esac
done

if [[ "x$buildtype" = "x" ]]; then
	buildtype="debug"
fi

MESONARG="$MESONARG --buildtype=$buildtype"

echo "MESONARG: $MESONARG"
echo "CCFLAGS: $CCFLAGS"

meson setup build $MESONARG -Dc_args="$CCFLAGS" || exit 1
cd build
ninja || exit 1
