# This source file is part of the Aument language
# Copyright (c) 2021 the aument contributors
#
# Licensed under Apache License v2.0 with Runtime Library Exception
# See LICENSE.txt for license information

import glob
import os
all_files = []
for root, _, files in os.walk("src/"):
    for file in files:
        if file.endswith(".h") or file.endswith(".c"):
            path = os.path.join(root, file)
            all_files.append(path)
all_files.sort()
exclude = set(glob.glob('src/compiler/*') +
    glob.glob('src/lib/*') +
[
    'src/core/rt/stdlib_begin.h',
    'src/core/rt/stdlib_end.h',
    'src/core/rt/malloc/gc.c',
    'src/core/rt/malloc/static.c',
    'src/main.c',
    'src/platform/spawn.c',
    'src/platform/tmpfile.c',
    'src/platform/cc.c',
    'src/core/rt/struct/helper.h',
    'src/core/rt/struct/helper.c',
    'src/core/stdlib/io.c',
    'src/core/stdlib/math.c',
])
all_files = filter(lambda x: x not in exclude, all_files)
print('\n'.join(all_files), end='')
