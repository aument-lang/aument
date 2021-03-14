# Performance

*(This is a draft version of the document.)*

Although at its core, Aument is meant to be a dynamic scripting language, it does not try to sacrifice speed for ease of usability. Below are a few design decisions (language or implementation wise), that will (hopefully) make Aument fast.

## Language design

### Static names

All names in Aument (identifiers, functions and classes) are static, predetermined at parse time and constant at runtime.

### [Classes](docs/architecture.md#classes)

Classes in Aument are flat structures, accessing their internal values is as fast as a pointer dereference.

## Implementation

### One-pass parser

The Aument parser has only one pass which generates virtual machine bytecode from a source file.

### Compact, uniform bytecode

Each bytecode instruction the virtual machine executes has a clear, uniform structure: a 32-bit value with 1-2 bytes per operand. Decoding each instruction is simple.

### Computed gotos

The virtual machine uses computed gotos for fast function dispatching.

### [NaN tagging](docs/architecture.md#nan-tagging)

The virtual machine and software compiled by Aument will use NaN tagging to represent dynamically typed values.


