# A brief tour of aulang

## Primitive values

Like many scripting languages, aulang supports the usual core data types: integers (`3`), floats (`3.14`), strings (`"Hello World"`) and booleans (`true` and `false`).

For strings, standard escape sequences work (only `\n` is implemented however).

You can also do some binary operations, add `+`, subtraction `-`, multiply `*`, division `/` and modulo `%` are currently supported.

```
1 + 1; // Results in 2
```

Comparisons (`<`, `>`, `<=`, `>=`, `==`, `!=` operators) work as you'd expect:

```
1 < 2; // (true)
```

You can also use boolean operators (logical and `&&` and logical or `||`):

```
true && false; // false
```

## Arrays and tuples

You can define arrays by using the literal syntax:

```
a = [1,2,3];
```

Tuples are like statically sized arrays:

```
a = #[1,2,3];
```

You can index and set an item in an array or a tuple. Like C and Python, aulang's collections begin at index 0.

```
a[0]; // 1
a[0] = 100;
a[0]; // 100
```

## Variables

You can assign values into variables like so:

```
pi = 3.14;
greeting = "Hello World!";
```

Note that all variables are local to the function they belong in. You cannot use variables outside of that function:

```
y = 1;
x = 0;
def local() {
    y = 0;
    print y;
}
local(); // 1
print y; // 0
```

This includes the top-level scope as well. In the example above, the function `local` cannot access the variable `x`.

**Note:** for thread-safety reasons, global variables don't exist. When it is implemented, you'll be able to pass values through channels or share thread-safe values.

## Control Flow

You can use `if` statements like how it works in C. Notice that there are no brackets surrounding the condition.

```
if 1 + 1 == 2 {
    print "The computer is working normally.";
} else {
    print "The computer is broken!";
}
```

The if statement checks if the condition is "truthy", converting the condition into a boolean and checks if it's true. Statements like this are valid:

```
if "string" {
    print "string is true";
}
```

See the [`bool` function documentation](./au-stdlib.md#bool) for boolean conversion.

aulang also has while loops:

```
while true {
    print "Forever";
}
```

## Input/output

To print something to the screen, use the `print` statement:

```
print "Hello World!\n";
```

You can also print multiple objects:

```
print "The answer to life, universe and everything is ", 42, "\n";
```

## Functions

To define a function, use the `def` statement. Inside functions, you can use `return` to return a value.

```
def y(x) {
    return x + 2;
}
print y(1); // 3
```

aulang's standard library provides some useful built-in functions. See the [stdlib reference manual](./au-stdlib.md) for more details.

## Modules

You can import files using the `import` statement. Note that all files are executed separately and you cannot directly use an imported file's variables/functions (unless exported).

```
// importee.au
print "Hello World\n";
```

```
// importer.au
import "./importee.au"; // prints out Hello World
```

To export a function, use the `export` statement:

```
// importee.au
export def random() {
    return 4;
}
```

Exported functions and variables are accessible under a **module**. You have to explicit import a file as a module in order to use it:

```
import "importee.au" as module;
print module::random(); // => 4
```
