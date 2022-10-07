/*
 * Copyright (C) 2021-2022 Nils Asmussen, Barkhausen Institut
 *
 * This file is part of M3 (Microkernel-based SysteM for Heterogeneous Manycores).
 *
 * M3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * M3 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include <base/Common.h>

#include "debug.h"

typedef void *FILE;

EXTERN_C FILE *const stderr = NULL;

EXTERN_C int fputs(const char *str, FILE *) {
    DebugBuf db;
    debug_new(&db);
    debug_puts(&db, str);
    debug_flush(&db);
    return 0;
}

EXTERN_C int fputc(int c, FILE *) {
    char str[] = {(char)c, '\0'};
    return fputs(str, NULL);
}

EXTERN_C size_t fwrite(const void *str, UNUSED size_t size, size_t nmemb, FILE *) {
    DebugBuf db;
    debug_new(&db);
    // assert(size == 1);
    const char *s = reinterpret_cast<const char *>(str);
    while(nmemb-- > 0)
        debug_putc(&db, *s++);
    debug_flush(&db);
    return 0;
}

// for __verbose_terminate_handler from libsupc++
EXTERN_C ssize_t write(int, const void *data, size_t count) {
    return static_cast<ssize_t>(fwrite(data, 1, count, NULL));
}
