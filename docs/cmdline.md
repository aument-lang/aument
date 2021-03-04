# Command line options

aulang can be invoked in the following way:

```
aulang [command] [options] file...
```

This section documents aulang's commands.

The section below was generated automatically (devs: *gen_help.py*).
Please don't modify it by hand!

## `build`: builds source code into a single binary

### Usage

```
aulang build input-file output-file
```

### Description

Builds *input-file* into an executable binary named *output-file*.

aulang compiles code by invoking a C compiler. By default, this is *gcc*,
however you can have aulang use another compiler by specifying it in
the `CC` environment variable.

aulang invokes the C compiler with the following arguments:

```
-flto -O2
```

Passing `-b` will make aulang output bytecode before it is compiled to C.

Passing `-c` will make aulang write C code into *output-file* instead
of outputting a compiled binary.

Passing `-g` will add the `-g` flag to the C compiler call.

## `help`: shows this help screen

### Usage

```
aulang help [command]
```

### Description

Shows the documentation for a specified command if given.
Else, a general help screen is shown.

## `run`: runs a program

### Usage

```
aulang run input-file
```

### Description

Runs *input-file* through an interpreter.

Passing `-b` will make aulang output bytecode before it is interpreted.

## `version`: print aulang version

### Usage

```
aulang version 
```

### Description

Prints aulang's current version number.

