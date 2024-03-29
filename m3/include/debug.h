/*
 * Copyright (C) 2021 Nils Asmussen, Barkhausen Institut
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

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

__attribute__((aligned(8))) typedef struct DebugBuf {
    char buf[128];
    size_t pos;
    size_t start;
} DebugBuf;

void debug_new(DebugBuf *db);
void debug_putc(DebugBuf *db, char c);
void debug_puts(DebugBuf *db, const char *str);
size_t debug_putu_rec(char *buf, size_t space, unsigned long long n, unsigned int base);
void debug_putu(DebugBuf *db, unsigned long long n, unsigned int base);
void debug_flush(DebugBuf *db);

#ifdef __cplusplus
}
#endif
