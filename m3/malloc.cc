#include <base/Common.h>

#include <debug.h>

extern "C" {
#include <features.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
}

#define DEBUG 0

void *__libc_malloc(size_t n) {
    return __libc_malloc_impl(n);
}

EXTERN_C void *malloc(size_t n) {
    void *res = __libc_malloc_impl(n);
#if DEBUG
    DebugBuf db;
    debug_new(&db);
    debug_puts(&db, "malloc(");
    debug_putu(&db, n, 10);
    debug_puts(&db, ") = 0x");
    debug_putu(&db, (ullong)res, 16);
    debug_puts(&db, "\n");
    debug_flush(&db);
#endif
    return res;
}

EXTERN_C void *realloc(void *p, size_t n) {
    void *res = __libc_realloc(p, n);
#if DEBUG
    DebugBuf db;
    debug_new(&db);
    debug_puts(&db, "realloc(0x");
    debug_putu(&db, (ullong)p, 16);
    debug_puts(&db, ", ");
    debug_putu(&db, n, 10);
    debug_puts(&db, ") = 0x");
    debug_putu(&db, (ullong)res, 16);
    debug_puts(&db, "\n");
    debug_flush(&db);
#endif
    return res;
}

EXTERN_C void *musl_calloc(size_t m, size_t n) {
    return __libc_calloc(m, n);
}

EXTERN_C void free(void *p) {
#if DEBUG
    DebugBuf db;
    debug_new(&db);
    debug_puts(&db, "free(0x");
    debug_putu(&db, (ullong)p, 16);
    debug_puts(&db, ")\n");
    debug_flush(&db);
#endif
    __libc_free(p);
}

extern "C" {
weak_alias(malloc, musl_malloc);
weak_alias(realloc, musl_realloc);
weak_alias(free, musl_free);
}
