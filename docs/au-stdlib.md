# aument standard library reference

The section below was generated automatically (devs: *gen_api.py*).
Please don't modify it by hand!

## Functions

### bool

Defined in *src/stdlib/types.h*.

Converts an object into a boolean

 * For *integer* and *floating-point* inputs, the result is `true` when the input is greater than 0.
 * For *boolean* inputs, the result is exactly the same as the input.
 * For *string* inputs, the result is `false` when the string is empty, otherwise it's true.
 * For all other inputs, the result is 0.

#### Arguments

 * **input:** Object to be converted into a boolean

#### Return value

The boolean equivalent of the `input` object.

### gc::heap_size

Defined in *src/stdlib/gc.h*.

Get the size of the heap.

#### Arguments

*none*

#### Return value

size of the heap

### input

Defined in *src/stdlib/io.h*.

Read a string from standard input without a newline character

#### Arguments

*none*

#### Return value

string read from stdin

### int

Defined in *src/stdlib/types.h*.

Converts an object into an integer.

 * For *integer* inputs, the result is exactly the same as the input.
 * For *float* inputs, the result is its integer equivalent.
 * For *string* inputs, the result is the base-10 conversion of the string.
 * For *boolean* inputs, the result is 1 if `true`, 0 if `false`.
 * For all other inputs, the result is 0.

#### Arguments

 * **input:** Object to be converted into integer

#### Return value

The integer equivalent of the `input` object.

### int

Defined in *src/stdlib/types.h*.

Converts an object into an float.

 * For *int* inputs, the result is its float equivalent.
 * For *float* inputs, the result is exactly the same as the input.
 * For *string* inputs, the result is the base-10 conversion of the string.
 * For *boolean* inputs, the result is 1 if `true`, 0 if `false`.
 * For all other inputs, the result is 0.

#### Arguments

 * **input:** Object to be converted into integer

#### Return value

The integer equivalent of the `input` object.

### io::close

Defined in *src/stdlib/io.h*.

Closes a file

#### Arguments

 * **file:** file object to be closed

#### Return value

*none*

### io::open

Defined in *src/stdlib/io.h*.

Opens a file

#### Arguments

 * **file:** path of file to be opened
 * **perm:** fopen permissions

#### Return value

file object

### len

Defined in *src/stdlib/collection.h*.

Gets the length of a collection

 * For structures (arrays), calling len will use the structure's internal length function.
 * For strings, it returns the number of UTF-8 codepoints in the string
 * For all other inputs, the result is 0.

#### Arguments

 * **collection:** The collection (an array, string,...)

#### Return value

The length of the collection.

### str

Defined in *src/stdlib/types.h*.

Converts an object into a string

#### Arguments

 * **input:** Object to be converted into string

#### Return value

The string equivalent of the `input` object.
