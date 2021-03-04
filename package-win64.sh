#!/bin/bash
# This source file is part of the aulang project
# Copyright (c) 2021 the aulang contributors
#
# Licensed under Apache License v2.0 with Runtime Library Exception
# See LICENSE.txt for license information

cd build
rm -f aulang-win64.zip
zip -9 aulang-win64.zip aulang.exe libau_runtime.a