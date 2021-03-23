#!/bin/bash
# This source file is part of the Aument project
# Copyright (c) 2021 the Aument contributors
#
# Licensed under Apache License v2.0 with Runtime Library Exception
# See LICENSE.txt for license information

rm -rf build
mkdir -p build
python3 ./build-scripts/gen_tests.py || exit 1
meson setup build $MESONARG -Dtests=true -Dmath_library=false -Dc_args="$CCFLAGS" || exit 1
cd build
meson test
