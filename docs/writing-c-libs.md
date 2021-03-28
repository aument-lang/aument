# Writing a C library for Aument

First, download the [library template](https://github.com/aument-lang/library-template) and try to build it. This tutorial assumes that you're using this build system.

## Entry point

All external modules in Aument start with the `au_extern_module_load` function with the following signature:

```c
au_extern_module_t
au_extern_module_load(struct au_extern_module_options *options);
```

The function takes an options argument, which specifies how the Aument script loads the external module.

The function must return an `au_extern_module_t` instance, or a null pointer if the external module doesn't export anything.

## Declaring a function

Use the `AU_EXTERN_FUNC_DECL` macro to declare a function that can be imported from Aument:

```c
AU_EXTERN_FUNC_DECL(lib_rand) {
    return au_value_int(4);
}
```

Once you have declared a function, you'll have to export it in the `au_extern_module_t` created in the module's entry point using [`au_extern_module_add_fn`](https://github.com/aument-lang/aument/blob/main/docs/c-api.md#au_extern_module_add_fn):

```c
au_extern_module_t
au_extern_module_load(struct au_extern_module_options *options) {
    au_extern_module_t data = au_extern_module_new();
    au_extern_module_add_fn(data, "rand", lib_rand, 0);
    return data;
}
```

Arguments passed into the function are stored in the `_args` parameter:

```c
AU_EXTERN_FUNC_DECL(lib_identity) {
    return _args[0];
}
```

To export a function that takes in an argument, change the `num_args` parameter passed into  `au_extern_module_add_fn` accordingly:

```c
au_extern_module_add_fn(data, "identity", lib_identity, 1);
```

## Using Aument values

From here, you can use the public functions documented in [c-api.md](https://github.com/aument-lang/aument/blob/main/docs/c-api.md) to manipulate Aument values.

### A note on reference counting

Aument uses reference counting for memory management. The library must follow the same rules to allocate memory that is shared between Aument and C.

To increase a reference count of a value, use the `au_value_ref` function.

To decrease a reference count of a value, use the `au_value_deref` function.

When a function returns, you'll have to call the deref functions on all parameters that aren't passed elsewhere. You must not call deref on the return value.

## Memory management

Aument exports memory management functions:

 * For objects, there are `au_obj_malloc` for allocation and `au_obj_free` for deallocation
 * For data, there are `au_data_malloc` for allocation and `au_data_free` for deallocation

You must use these functions for values shared between Aument and the native library. Aument objects such as strings, classes, tuples and arrays must be allocated using the `au_obj_malloc` function.

For internal data, you may use other memory management functions like the ones in the C standard library (`malloc` and `free`). Note that Aument is not aware of those functions and may report wrong numbers when calling memory-related functions such as [`gc::heap_size`](https://github.com/aument-lang/aument/blob/main/docs/au-stdlib.md#gcheap_size).
