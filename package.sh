#!/bin/bash
# This source file is part of the aument project
# Copyright (c) 2021 the aument contributors
#
# Licensed under Apache License v2.0 with Runtime Library Exception
# See LICENSE.txt for license information

main_dir="$(pwd)"
mkdir -p dist
rm -f dist/*

MESONARG="-Dstatic_exe=true" CC=musl-gcc ./build.sh -f release
cd build
rm -f aument-linux64.zip
cp "$main_dir/docs/package/README-linux.md" .
zip -9 aument-linux64.zip aument aument.h libau_runtime.a README-linux.md
cd ..
mv build/aument-linux64.zip dist/

./build.sh -f release --cross-win64
cd build
rm -f aument-win64.zip
cp "$main_dir/docs/package/README-windows.md" .
zip -9 aument-win64.zip aument.exe aument.h libaument.exe.a libau_runtime.a README-windows.md
cd ..
mv build/aument-win64.zip dist/

md5sum dist/aument-linux64.zip > dist/aument-linux64.md5
md5sum dist/aument-win64.zip > dist/aument-win64.md5
