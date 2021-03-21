# A brief tour of Aument

## Primitive values

Like many scripting languages, Aument supports the usual core data types: integers (`3`), floats (`3.14`), strings (`"Hello World"`), booleans (`true` and `false`) and a none value (`nil`).

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

Tuples are statically sized arrays, you cannot remove items from it:

```
a = #[1,2,3];
```

You can index and set an item in an array or a tuple. Like C and Python, Aument's collections begin at index 0.

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

All variables are local to the function they belong in. You cannot use variables outside of that function:

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

### Dynamic types, static names

Identifiers, function names and class names in Aument are static: they are resolved at parsing time. As such, the concept of global variables do not exist in Aument.

In order to share states through function calls, you'll have to pass variables directly or wrap them in a class.

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

See the [`bool` function documentation](./au-stdlib.md#bool) for more details on boolean conversion.

Aument also has while loops:

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

Aument's standard library provides some useful built-in functions. See its [reference manual](./au-stdlib.md) for more details.

Functions have names that are fixed at parse time, and the number of arguments they take is always constant. If you're trying to call or define a function, and Aument can't find it:

```
def mistype() {}
misttype();
```

It will error out directly after parsing:

```
parser error(3) in /tmp/mistype.au: unknown function misttype
2 | misttype();
```

Of course, you can use a function that is declared later in the source file:

```
a();
def a(){}
```

## Classes

You can define a compound data type, a *class* using the `class` keyword:

```
class Human {
    var name;
}
```

Here, we define a class named `Human`, that holds a private variable `name`.

You can define a empty class like so:

```
class EmptyClass;
```

Classes are like any other value. You can create an empty one using the `new` keyword:

```
alice = new Human;
```

Just like functions, if you try to use a undeclared class, it will error out after parsing.

### Methods and private variables

To modify or access a private variable in a class instance, you'll need to declare a *method*, a function that can only be called if the first argument's type matches that of the class:

```
def (self: Human) init(name) {
    @name = name;
}
```

Here, we declare the function `init`, that takes 2 arguments, `self` (a `Human` class instance), and `name` (any dynamically typed variable). You can call `init` like any other function:

```
init(alice, "Alice");
```

You cannot access private variables of imported classes.

#### Dynamic dispatch

Methods can also be dynamically dispatched:

```
def (self: Human) say() {
    print "I'm ", @name, "\n";
}
class Cat {}
def (self: Cat) say() {
    print "meow!\n";
}
cat = new Cat;
say(alice);
say(cat);
```

Based on the type of the first argument, an Aument program will choose which function to call at runtime. When we call `say(alice);`, since the program sees that we've passed a `Human` type, it chooses to forward the call to the first `say` function, which prints out:

```
I'm Alice
```

Likewise, when we call `say(cat);`, it dispatches the call to the second `say` function, giving us:

```
meow!
```

### Dot calls

You can use the dot operator `.` to call a function, such that the left-hand side of the operation is the first argument of the function:

```
alice.say();
```

Is equivalent to:

```
say(alice);
```

### Dot binding

You can also use the dot operator to bind an argument to a function. Bound functions must be called using the `.(` operator:

```
def add(x,y){ return x + y; }
add_5 = (5).add;
print add_5.(10);
```

Outputs:

```
15
```

By omitting the left-hand side in a dot binding expression, you can create an unbounded function value:

```
def double(x) { return x * 2; }
op = .double;
print op.(10);
```

## Modules

You can import files using the `import` statement.

```
// importee.au
print "Hello World\n";
```

```
// importer.au
import "./importee.au"; // prints out Hello World
```

All files are executed separately and you cannot directly use an imported file's variables/functions (unless exported). To export a function, use the `export` statement:

```
// importee.au
export def random() {
    return 4;
}
```

You can also export a class:

```
export class Human {
    val name;
}
```

Exported functions and classes are accessible under a **module**. You have to explicitly import a file as a module in order to use it:

```
import "importee.au" as module;
print module::random(); // => 4
```

### Importing native dynamically-linked library (DLLs)

You can also import a dynamically linked library. On Unix systems, these files end with `.so`. On Windows, these files end with `.dll`. See [`tests/dl-module`](/tests/dl-module) for an example of importing a C library from Aument.

If the `.lib` extension is used, Aument will load the library with the extension corresponding to the platform's library extension.

If a DLL supports subpath imports, the importer can specify which subpath they want to import:

```
import "./libmodule.dll:subpath";
```

The example above imports the module in the library `./libmodule.dll` specified by the subpath `subpath`.
