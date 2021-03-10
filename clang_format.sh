#!/bin/bash
find src tests -regex '.*\.\(c\|h\)' -exec clang-format -style=file -i {} \;
