#!/bin/bash
# This source file is part of the aulang project
# Copyright (c) 2021 the aulang contributors
#
# Licensed under Apache License v2.0 with Runtime Library Exception
# See LICENSE.txt for license information

rm -rf build
mkdir -p build
python3 gen_tests.py
meson setup build $MESONARG -Dtests=enabled -Dc_args="$CCFLAGS"
cd build
ninja
./aulang-tests