#!/bin/bash
# This source file is part of the aument project
# Copyright (c) 2021 the aument contributors
#
# Licensed under Apache License v2.0 with Runtime Library Exception
# See LICENSE.txt for license information

mkdir -p dist
rm -f dist/*

./build.sh -f release
cd build
rm -f aument-linux64.zip
zip -9 aument-linux64.zip aument libau_runtime.a
cd ..
mv build/aument-linux64.zip dist/

./build.sh -f release --cross-win64
cd build
rm -f aument-win64.zip
zip -9 aument-win64.zip aument.exe libau_runtime.a
cd ..
mv build/aument-win64.zip dist/
