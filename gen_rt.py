import sys

output = sys.argv[1]
ident = sys.argv[2]
files = sys.argv[3:]
total = ""
for fn in files:
    with open(fn, "r") as f:
        total += f.read()
total_bytes = total.encode('ascii')
values = ','.join(map(str, total_bytes))
with open(output, "w") as f:
    f.write("#include <stdlib.h>\n")
    f.write("const char %s[] = {%s};\n" % (ident, values))
    f.write("const size_t %s_LEN = %d;\n" % (ident, len(total_bytes)))
