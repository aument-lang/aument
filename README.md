# aulang

What if we had a scripting language that...

  * **has minimal dependencies:** you only need a C compiler to build the aulang's interpreter.
  * **is fast in development time:** like Python and Javascript, you can simply write code and run it directly.
  * **is fast in deployment:** once finished, you can compile your code into a lightweight executable. You don't even need any external runtime to run it!

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

Note that the build command requires a C compiler in the environment's `PATH` (default is `gcc`).

## Building

### Requirements

aulang currently only supports Linux.

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

## Documentation

  * [command line reference](./docs/cmdline.md)
  * [aulang standard library reference](./docs/au-stdlib.md)
  * [aulang C API reference](./docs/c-api.md)

## Changelog

This project uses the [semver](https://semver.org/spec/v2.0.0.html) versioning scheme.

See `CHANGELOG.md`.

## License

This implementation of aulang is licensed under Apache License v2.0 with Runtime Library Exception. See `LICENSE.txt` for more details.

## Authors

See `ACKNOWLEDGEMENTS.txt`.
