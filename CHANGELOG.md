# Version 0.5 [α]

  * **Breaking:** rewrote Git history. I promise I won't do this again.
  * Language features:
    * Support compound assignment operators with indexing (`x[y] += z;`)
    * Added character strings (automatically turns `"A"c` into 65).
    * Support new expression with initializer
    * Added bitwise AND `&`, bitwise OR `|`, bitwise NOT `~`, shift left `<<`, shift right `>>` operators
  * Standard library:
    * Moved type conversion functions into modules (`int` to `int::into`, ...), turning `bool`, `float` and `int` into fully fledged modules.
    * Added `code_points`, `bytes`, `index_of`, `contains`, `starts_with`, `ends_with` functions to str module
    * Added `array::push`, `array::pop`, `array::repeat` functions
    * Added `is` type-check functions to `bool`, `float`, `int`, `str` modules.
    * Moved `len` function to `list` module.
    * Moved `input` function to `io` module.
  * Interpreter:
    * Better error handling with stack traces.
    * Interpreter now correctly errors out on module import failure.
  * Libraries:
    * Reorganized C API
  * Parser:
    * Use strtod for parsing floating-point values

# Version 0.4 [α]

  * Language features:
    * Added function values
    * Dot binding and dot calls for creating and calling function values
    * Importing `.dll` libraries on Windows, `.so` libraries on Unix
  * Standard library:
    * `io`: Added `read`, `read_up_to`, `write`, `flush` functions
    * Enabled `math` functions
    * Fixed reference counting bugs
  * Interpreter:
    * Implemented some performance improvements
      * Storing the VM's instruction pointer in a register
      * Prefetching subsequent instruction after decoding
  * Parser:
    * Better code generation by eliding local loads

# Version 0.2–0.3 [α]

  * **Birth of the Aument language**
  * General:
    * Additional standard library functions
    * Better parser/interpreter errors
    * Better code coverage and testing
  * Language features:
    * Added classes
    * Arrays, tuples and syntax to index them
    * Methods and dynamic dispatch
  * Parser changes:
    * Added unary-not expression
    * Exporting data and importing from source file/dynamic libraries
    * Forward declaration of classes and functions
  * Interpreter:
    * Minor performance improvements
    * Deferred reference count feature for faster VM operations
    * Specialized math opcodes for integers and floats
  * C compiler:
    * Full feature parity with interpreter

# Version 0.1 [pre-α]

  * Floating point support
  * One-line comment support
  * Add `true`, `false` values.
  * Added standard library functions `bool`, `input`, `int`, `str`
  * *under the hood:* Replace hash function with FNV hash
  * *under the hood:* use NaN tagging for value representation (if possible)

# Version 0.0 [pre-α]

  * Core functionality including:
    * Basic data structures: Integers, strings and booleans
    * Basic statements, control flow and function definition
    * File importing
    * Code compilation to binary through C
  * Tested platform: Ubuntu 20.04

