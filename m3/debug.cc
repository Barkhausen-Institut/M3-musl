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
#if !defined(__host__)
#   include <base/Env.h>
#endif

#include <debug.h>

#if !defined(__host__)
EXTERN_C void gem5_writefile(const char *str, uint64_t len, uint64_t offset, uint64_t file);
#endif

void debug_new(DebugBuf *db) {
    db->pos = 0;
#if !defined(__host__)
    debug_puts(db, "[        @");
    debug_putu(db, m3::env()->tile_id, 16);
    debug_puts(db, "] ");
#endif
}

void debug_puts(DebugBuf *db, const char *str) {
    char c;
    while((c = *str++))
        db->buf[db->pos++] = c;
}

static size_t debug_putu_rec(DebugBuf *db, ullong n, uint base) {
    static char hexchars_small[]   = "0123456789abcdef";
    size_t res = 0;
    if(n >= base)
        res += debug_putu_rec(db, n / base, base);
    db->buf[db->pos + res] = hexchars_small[n % base];
    return res + 1;
}

void debug_putu(DebugBuf *db, ullong n, uint base) {
    db->pos += debug_putu_rec(db, n, base);
}

void debug_flush(DebugBuf *db) {
#if !defined(__host__)
    static const char *fileAddr = "stdout";
    gem5_writefile(db->buf, db->pos, 0, reinterpret_cast<uint64_t>(fileAddr));
#endif
    db->pos = 0;
}
