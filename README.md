# aulang

What if we had a scripting language with...

  * **minimal dependencies:** you only need a C compiler to build the aulang's interpreter.
  * **fast development time:** like Python and Javascript, you can simply write code and run it directly.
  * **fast run time:** once finished, you can compile your code into a lightweight executable. You don't even need any external runtime to run it!

Welcome to **aulang** (currently in pre-alpha).

## Syntax

An example fibonacci program:

```
def fib(n) {
    if n <= 1 {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

print fib(28);
```

See [a brief tour](./docs/tour.md) for more.

## Usage

Once aulang is compiled, you can run a program in interpreter mode with `aulang run [file]`

```
$ cat hello.au
print "Hello World\n";
$ ./aulang run hello.au
Hello World
```

You can also build the program as an executable with `aulang build [file] [output]`

```
$ ./aulang build hello.au /tmp/hello && /tmp/hello
Hello World
```

The build command requires a C compiler in the environment's `PATH` (default is `gcc`). If the environment variable `CC` is specified, it will use that instead.

## Building

### Requirements

  * A C compiler (tested with GCC v9.3.0)
  * Meson (tested with 0.53.2)
  * Python

### Linux

In the source directory, run the `build.sh` script. For debug builds, do:

```
./build.sh debug
```

For release builds, do:

```
./build.sh release
```

An executable `aulang` will appear in the `build` directory.

### Windows

**For Cygwin users,** install gcc on Cygwin, open a Cygwin shell in the current directory and follow the Linux instruction.

**For MSVC users,** run `build.bat` (for release mode). Note that aulang's compiler feature is disabled because MSVC doesn't support some Unix functions.

## Documentation

  * [brief tour](./docs/tour.md)
  * [command line reference](./docs/cmdline.md)
  * [aulang standard library reference](./docs/au-stdlib.md)
  * [aulang C API reference](./docs/c-api.md)

## Contributing

aulang is in alpha development, so feel free to ask any questions or suggest features in the repository's [issues page](https://github.com/chm8d/aulang/issues/).

## Changelog

This project uses the [semver](https://semver.org/spec/v2.0.0.html) versioning scheme.

See `CHANGELOG.md`.

## License

This implementation of aulang is licensed under Apache License v2.0 with Runtime Library Exception. See `LICENSE.txt` for more details.

## Authors

See `ACKNOWLEDGEMENTS.txt`.
