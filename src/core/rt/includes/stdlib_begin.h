#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef AU_FEAT_LIBDL
#include <dlfcn.h>
#endif

#define COPY_VALUE(dest, src)                                             \
    do {                                                                  \
        const au_value_t old = dest;                                      \
        dest = src;                                                       \
        au_value_ref(dest);                                               \
        au_value_deref(old);                                              \
    } while (0)
#define MOVE_VALUE(dest, src)                                             \
    do {                                                                  \
        const au_value_t old = dest;                                      \
        dest = src;                                                       \
        au_value_deref(old);                                              \
    } while (0)

#define AU_EXTERN_FUNC_DECL(NAME)                                         \
    au_value_t NAME(struct au_vm_thread_local *, const au_value_t *)