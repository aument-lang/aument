# C API

The section below was generated automatically (devs: *gen_api.py*).
Please don't modify it by hand!

## Functions

### au_bc_dbg

```c
void au_bc_dbg(const struct au_bc_storage *bcs,
               const struct au_program_data *data);
```

Defined in *src/core/bc.h*.

Debugs an bytecode storage container

#### Arguments

 * **bcs:** the bytecode storage
 * **data:** program data

#### Return value

*none*

### au_bc_storage_del

```c
void au_bc_storage_del(struct au_bc_storage *bc_storage);
```

Defined in *src/core/bc.h*.

Deinitializes an au_bc_storage instance

#### Arguments

 * **bc_storage:** instance to be deinitialized

#### Return value

*none*

### au_bc_storage_init

```c
void au_bc_storage_init(struct au_bc_storage *bc_storage);
```

Defined in *src/core/bc.h*.

Initializes an au_bc_storage instance

#### Arguments

 * **bc_storage:** instance to be initialized

#### Return value

*none*

### au_c_comp

```c
void au_c_comp(struct au_c_comp_state *state,
               const struct au_program *program);
```

Defined in *src/compiler/c_comp.h*.

Compiles a program into file specified by an au_c_comp_state instance

#### Arguments

*none*

#### Return value

*none*

### au_c_comp_state_del

```c
void au_c_comp_state_del(struct au_c_comp_state *state);
```

Defined in *src/compiler/c_comp.h*.

Deinitializes an au_c_comp_state instance

#### Arguments

 * **state:** instance to be initialized

#### Return value

*none*

### au_fn_del

```c
void au_fn_del(struct au_fn *fn);
```

Defined in *src/core/program.h*.

Deinitializes an au_fn instance

#### Arguments

 * **fn:** instance to be initialized

#### Return value

*none*

### au_hash

```c
hash_t au_hash(const uint8_t *str, const size_t len);
```

Defined in *src/core/hash.h*.

Hashes a chunk of memory `str` with length `len`

#### Arguments

*none*

#### Return value

*none*

### au_mmap_del

```c
void au_mmap_del(struct au_mmap_info *info);
```

Defined in *src/platform/mmap.h*.

Deinitializes an au_mmap_info instance

#### Arguments

 * **info:** instance to be deinitialized

#### Return value

*none*

### au_mmap_read

```c
int au_mmap_read(const char *path, struct au_mmap_info *info);
```

Defined in *src/platform/mmap.h*.

Loads a file into memory and stores a reference into a au_mmap_info instance

#### Arguments

 * **path:** path of file
 * **info:** info

#### Return value

1 if success, 0 if errored

### au_parse

```c
struct au_parser_result au_parse(const char *src, size_t len,
                                 struct au_program *program);
```

Defined in *src/core/parser/parser.h*.

Parses source code into an au_program instance

#### Arguments

 * **src:** source code string
 * **len:** the bytesize len of the source code
 * **program:** output into a program

#### Return value

1 if parsed successfully, 0 if an error occurred

### au_program_data_add_data

```c
int au_program_data_add_data(struct au_program_data *p_data,
                             au_value_t value, uint8_t *v_data,
                             size_t v_len);
```

Defined in *src/core/program.h*.

Adds constant value data into a au_program_data instance

#### Arguments

 * **p_data:** au_program_data instance
 * **value:** au_value_t representation of constant
 * **v_data:** byte array of internal constant data
 * **v_len:** size of `v_data`

#### Return value

index of the data

### au_program_data_del

```c
void au_program_data_del(struct au_program_data *data);
```

Defined in *src/core/program.h*.

Deinitializes an au_program_data instance

#### Arguments

 * **data:** instance to be deinitialized

#### Return value

*none*

### au_program_data_init

```c
void au_program_data_init(struct au_program_data *data);
```

Defined in *src/core/program.h*.

Initializes an au_program_data instance

#### Arguments

 * **data:** instance to be initialized

#### Return value

*none*

### au_program_dbg

```c
void au_program_dbg(const struct au_program *p);
```

Defined in *src/core/program.h*.

Debugs an au_program instance

#### Arguments

*none*

#### Return value

*none*

### au_program_del

```c
void au_program_del(struct au_program *p);
```

Defined in *src/core/program.h*.

Initializes an au_program instance

#### Arguments

 * **p:** instance to be deinitialized

#### Return value

*none*

### au_vm_exec_unverified

```c
au_value_t au_vm_exec_unverified(struct au_vm_thread_local *tl,
                                 const struct au_bc_storage *bcs,
                                 const struct au_program_data *p_data,
                                 const au_value_t *args);
```

Defined in *src/core/vm.h*.

Executes unverified bytecode in a au_bc_storage

#### Arguments

 * **tl:** thread local storage
 * **bcs:** the au_bc_storage to be executed. Bytecode stored here is unverified and should be checked beforehand if it's safe to run.
 * **p_data:** global program data
 * **args:** argument array

#### Return value

return value specified by interpreted aulang's return statement

### au_vm_exec_unverified_main

```c
static inline au_value_t
au_vm_exec_unverified_main(struct au_vm_thread_local *tl,
                           struct au_program *program);
```

Defined in *src/core/vm.h*.

Executes unverified bytecode in a au_program

#### Arguments

 * **tl:** thread local storage
 * **program:** the au_program to be executed. Bytecode stored here is unverified and should be checked beforehand if it's safe to run.

#### Return value

return value

### au_vm_thread_local_del

```c
void au_vm_thread_local_del(struct au_vm_thread_local *tl);
```

Defined in *src/core/vm.h*.

Deinitializes an au_vm_thread_local instance

#### Arguments

 * **tl:** instance to be deinitialized

#### Return value

*none*

### au_vm_thread_local_init

```c
void au_vm_thread_local_init(struct au_vm_thread_local *tl,
                             const struct au_program_data *p_data);
```

Defined in *src/core/vm.h*.

Initializes an au_vm_thread_local instance

#### Arguments

 * **tl:** instance to be initialized
 * **p_data:** global au_program_data instance

#### Return value

*none*
