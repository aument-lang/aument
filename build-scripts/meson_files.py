import glob
import os
all_files = []
for root, _, files in os.walk("src/"):
    for file in files:
        if file.endswith(".h") or file.endswith(".c"):
            path = os.path.join(root, file)
            all_files.append(path)
all_files.sort()
exclude = set(glob.glob('src/compiler/*') + [
    'src/core/rt/stdlib_begin.h',
    'src/core/rt/stdlib_end.h',
    'src/core/rt/malloc.c',
    'src/main.c',
    'src/platform/spawn.c',
    'src/platform/tmpfile.c',
    'src/platform/cc.c',
    'src/core/rt/struct/helper.h',
    'src/core/rt/struct/helper.c',
])
all_files = filter(lambda x: x not in exclude, all_files)
print('\n'.join(map(lambda s: "'%s',"%s, all_files)))
