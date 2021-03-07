#!/bin/bash
# This source file is part of the aulang project
# Copyright (c) 2021 the aulang contributors
#
# Licensed under Apache License v2.0 with Runtime Library Exception
# See LICENSE.txt for license information

MESONARG="$MESONARG -Db_coverage=true" ./test.sh || exit 1
cd build
ninja coverage-html && firefox $PWD/meson-logs/coveragereport/index.html