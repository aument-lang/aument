#!/bin/bash
find src -regex '.*\.\(c\|h\)' -exec clang-format -style=file -i {} \;