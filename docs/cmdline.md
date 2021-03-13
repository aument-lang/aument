# Command line options

aument can be invoked in the following way:

```
aument [command] [options] file...
```

This section documents aument's commands.

The section below was generated automatically (devs: *gen_help.py*).
Please don't modify it by hand!

## `build`: builds source code into a single binary

### Usage

```
aument build input-file output-file
```

### Description

Builds *input-file* into an executable binary named *output-file*.

aument compiles code by invoking a C compiler. By default, this is *gcc*,
however you can have aument use another compiler by specifying it in
the `CC` environment variable.

Unless the `--no-opt` parameter is passed, aument invokes the C compiler
with the following arguments:

```
-flto -O2
```

Passing `-b` will make aument output bytecode before it is compiled to C.

Passing `-c` will make aument write C code into *output-file* instead
of outputting a compiled binary.

Passing `-g` will add the `-g` flag to the C compiler call.

## `help`: shows this help screen

### Usage

```
aument help [command]
```

### Description

Shows the documentation for a specified command if given.
Else, a general help screen is shown.

## `run`: runs a program

### Usage

```
aument run input-file
```

### Description

Runs *input-file* through an interpreter.

Passing `-b` will make aument output bytecode before it is interpreted.

## `version`: print aument version

### Usage

```
aument version 
```

### Description

Prints aument's current version number.

