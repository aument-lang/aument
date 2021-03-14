# The Aument Language

The **Aument** language is a work-in-progress dynamically-typed scripting language with

  * **performance first:** this scripting language is designed with performance in mind. ([read more](/docs/design-for-performance.md))
  * **fast development time:** like Python and Javascript, you can write code and run it directly.
  * **fast deployment:** once finished, you can compile your code into a native, lightweight executable. You don't even need any *external* runtime to run it!

(Previously named the aulang language. The current name comes from the Italian word *aumento*, meaning growth, augment. It's pronounced /ˈɔː.mənt/.)

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

Once Aument is compiled, you can run a program in interpreter mode with `aument run [file]`

```
$ cat hello.au
print "Hello World\n";
$ ./aument run hello.au
Hello World
```

You can also build the program as an executable with `aument build [file] [output]`

```
$ ./aument build hello.au /tmp/hello && /tmp/hello
Hello World
```

The file `libau_runtime.a` must be in the same directory as `aument`. This will be statically linked to your executable.

The build command requires a C compiler in the environment's `PATH` (default is `gcc`). If the environment variable `CC` is specified, it will use that instead.

## Building

### Requirements

  * A C compiler (tested with GCC v9.3.0)
  * Meson (tested with 0.53.2)
  * Python 3

### Linux

In the source directory, run the `build.sh` script. For debug builds, do:

```
./build.sh debug
```

For release builds, do:

```
./build.sh release
```

An executable `aument` will appear in the `build` directory.

#### Install

You can use the `install.sh` script to install aument into `/usr/local`.

### Windows

If your build machine is Windows, install [Python](https://www.python.org/downloads/) and [Meson](https://mesonbuild.com/SimpleStart.html#windows1).

**For Cygwin users,** install gcc on Cygwin, open a Cygwin shell in the current directory and follow Linux instructions.

**For MSVC users,** run `build.bat` (for release mode).

If you're cross-compiling for Windows (64-bit), use the command:

```
./build.sh --cross-win64
```

## Documentation

  * [Brief tour](./docs/tour.md)
  * [Command line reference](./docs/cmdline.md)
  * [Standard library reference](./docs/au-stdlib.md)
  * [C API reference](./docs/c-api.md)
  * [Internal architecture](./docs/architecture.md)

## Contributing

This language is in alpha development, so feel free to ask any questions or suggest features in the repository's [issues page](https://github.com/aument-lang/aument/issues/).

## Changelog

This project uses the [semver](https://semver.org/spec/v2.0.0.html) versioning scheme.

See `CHANGELOG.md`.

## License

This implementation of aument is licensed under Apache License v2.0 with Runtime Library Exception. See `LICENSE.txt` for more details.

## Authors

See `ACKNOWLEDGEMENTS.txt`.
