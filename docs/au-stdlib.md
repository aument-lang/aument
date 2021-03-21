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

### io::flush

Defined in *src/stdlib/io.h*.

Flushes a file

#### Arguments

 * **file:** file object to be flushed

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

### io::read

Defined in *src/stdlib/io.h*.

Reads a string from a file

#### Arguments

 * **file:** path of file to be opened

#### Return value

string string read from file

### io::read_up_to

Defined in *src/stdlib/io.h*.

Reads a maximum of n bytes from a file

#### Arguments

 * **file:** path of file to be opened
 * **n:** number of bytes to read

#### Return value

string string read from file

### io::stderr

Defined in *src/stdlib/io.h*.

Opens stderr as a file object

#### Arguments

*none*

#### Return value

file object

### io::stdin

Defined in *src/stdlib/io.h*.

Opens stdin as a file object

#### Arguments

*none*

#### Return value

file object

### io::stdout

Defined in *src/stdlib/io.h*.

Opens stdout as a file object

#### Arguments

*none*

#### Return value

file object

### io::write

Defined in *src/stdlib/io.h*.

Writes a string to a file

#### Arguments

 * **file:** path of file to be opened
 * **string:** string to read to file

#### Return value

string bytes written

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

### math::abs

Defined in *src/stdlib/math.h*.

Returns the absolute value of a number

#### Arguments

 * **n:** a number (integer/float)

#### Return value

the absolute value of `n`

### math::acos

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::acosh

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::asin

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::asinh

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::atan

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::atan2

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::atanh

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::cbrt

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::ceil

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::cos

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::cosh

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::erf

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::erfc

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::exp

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::exp

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::floor

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::hypot

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::is_finite

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::is_infinite

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::is_nan

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::is_normal

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::lgamma

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::log10

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::log2

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::max

Defined in *src/stdlib/math.h*.

Returns the maximum value between 2 arguments

#### Arguments

 * **a:** a number (integer/float)
 * **b:** a number (integer/float)

#### Return value

maximum of a or b

### math::min

Defined in *src/stdlib/math.h*.

Returns the minimum value between 2 arguments

#### Arguments

 * **a:** a number (integer/float)
 * **b:** a number (integer/float)

#### Return value

minimum of a or b

### math::pow

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::round

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::sin

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::sinh

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::sqrt

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::tan

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::tanh

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::tgamma

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### math::trunc

Defined in *src/stdlib/math.h*.



#### Arguments

*none*

#### Return value

*none*

### str

Defined in *src/stdlib/types.h*.

Converts an object into a string

#### Arguments

 * **input:** Object to be converted into string

#### Return value

The string equivalent of the `input` object.
