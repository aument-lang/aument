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
        if file.endswith(".h"):
            path = os.path.join(root, file)
            all_files.append(path)
all_files.sort()
exclude = set(glob.glob('src/compiler/*') + [
    'src/core/rt/stdlib_begin.h',
    'src/core/rt/stdlib_end.h',
])
all_files = filter(lambda x: x not in exclude, all_files)
print('\n'.join(map(lambda s: "'%s',"%s, all_files)))
