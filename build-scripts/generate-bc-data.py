with open("./build-scripts/opcodes.txt", "r") as f:
    opcodes = f.readlines()

opcodes_h = open("./src/core/bc_data/opcodes.txt", "w")
opcodes_h.write("enum au_opcode {\n")

callbacks_h = open("./src/core/bc_data/callbacks.txt", "w")
callbacks_h.write("static void *cb[] = {\n")

dbg_h = open("./src/core/bc_data/dbg.txt", "w")
dbg_h.write("const char *au_opcode_dbg[AU_MAX_OPCODE] = {\n")

num_opcodes = 0
for opcode in opcodes:
    if opcode == "\n" or opcode.startswith("#"):
        continue
    opcode = opcode.strip()

    opcodes_h.write(f"AU_OP_{opcode} = {num_opcodes},\n")
    callbacks_h.write(f"&&CASE(AU_OP_{opcode}),\n")
    dbg_h.write(f"\"{opcode}\",\n")

    num_opcodes += 1

opcodes_h.write("};\n")
callbacks_h.write("};\n")
dbg_h.write("};\n")