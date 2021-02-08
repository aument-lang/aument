# aulang standard library reference

The section below was generated automatically (devs: *gen_api.py*).
Please don't modify it by hand!

## Functions

### bool

Defined in *src/stdlib/au_stdlib.h*.

Converts an object into a boolean

 * For *integer* and *floating-point* inputs, the result is `true` when the input is greater than 0.
 * For *boolean* inputs, the result is exactly the same as the input.
 * For *string* inputs, the result is `false` when the string is empty, otherwise it's true.
 * For all other inputs, the result is 0.

#### Arguments

 * **input:** Object to be converted into a boolean

#### Return value

The boolean equivalent of the `input` object.

### input

Defined in *src/stdlib/au_stdlib.h*.

Read a string from standard input without a newline character

#### Arguments

*none*

#### Return value

string read from stdin

### int

Defined in *src/stdlib/au_stdlib.h*.

Converts an object into an integer.

 * For *integer* inputs, the result is exactly the same as the input.
 * For *string* inputs, the result is the base-10 conversion of the string.
 * For *boolean* inputs, the result is 1 if `true`, 0 if `false`.
 * For all other inputs, the result is 0.

#### Arguments

 * **input:** Object to be converted into integer

#### Return value

The integer equivalent of the `input` object.

### str

Defined in *src/stdlib/au_stdlib.h*.

Converts an object into a string

#### Arguments

 * **input:** Object to be converted into string

#### Return value

The string equivalent of the `input` object.
