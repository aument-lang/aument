#!/bin/bash
# This source file is part of the aument project
# Copyright (c) 2021 the aument contributors
#
# Licensed under Apache License v2.0 with Runtime Library Exception
# See LICENSE.txt for license information

for i in "$@"
do
case $i in
    -f|--force)
        rm -rf build
        shift
    ;;
    *)
        shift
    ;;
esac
done

./build.sh release || exit 1
cd build
meson install 
