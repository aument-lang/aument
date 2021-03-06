#!/bin/bash
# This source file is part of the aulang project
# Copyright (c) 2021 the aulang contributors
#
# Licensed under Apache License v2.0 with Runtime Library Exception
# See LICENSE.txt for license information

mkdir -p dist
rm -f dist/*

./build.sh -f release
cd build
rm -f aulang-linux64.zip
zip -9 aulang-linux64.zip aulang libau_runtime.a
cd ..
mv build/aulang-linux64.zip dist/

./build.sh -f release --cross-win64
cd build
rm -f aulang-win64.zip
zip -9 aulang-win64.zip aulang.exe libau_runtime.a
cd ..
mv build/aulang-win64.zip dist/
