import os
import subprocess

files = list(os.walk("src"))
files += list(os.walk("tests"))
for root, _, files in files:
    for file in files:
        if file.endswith(".h") or file.endswith(".c"):
            subprocess.run([
                "clang-format",
                "-style=file",
                "-i",
                os.path.join(root,file)
            ])
