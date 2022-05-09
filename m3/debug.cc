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
#    include <base/Env.h>
#    include <base/TCU.h>
#    include <base/util/Math.h>
#endif

#include <debug.h>

#if defined(__host__)
#    include <bits/syscall.h>

#    include "../arch/x86_64/syscall_arch.h"
#else
EXTERN_C void gem5_writefile(const char *str, uint64_t len, uint64_t offset, uint64_t file);
#endif

void debug_new(DebugBuf *db) {
    db->pos = 0;
#if !defined(__host__)
    debug_puts(db, "[musl       @");
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
    static char hexchars_small[] = "0123456789abcdef";
    size_t res = 0;
    if(n >= base)
        res += debug_putu_rec(db, n / base, base);
    db->buf[db->pos + res] = hexchars_small[n % base];
    return res + 1;
}

void debug_putu(DebugBuf *db, ullong n, uint base) {
    db->pos += debug_putu_rec(db, n, base);
}

void print_tcu(const char *str, size_t len) {
    constexpr size_t PRINT_REGS = 32;
    constexpr size_t EXT_UNPRIV_REGS = 2 + 5;
    constexpr size_t EP_COUNT = 128;
    constexpr size_t PRINT_REG = 6;
    typedef uint64_t reg_t;

    len = m3::Math::min(len, PRINT_REGS * sizeof(reg_t) - 1);

    uintptr_t buffer = m3::TCU::MMIO_ADDR + (EXT_UNPRIV_REGS + EP_COUNT * 3) * sizeof(reg_t);
    const reg_t *rstr = reinterpret_cast<const reg_t *>(str);
    const reg_t *end = reinterpret_cast<const reg_t *>(str + len);
    while(rstr < end) {
        m3::CPU::write8b(buffer, *rstr);
        buffer += sizeof(reg_t);
        rstr++;
    }

    m3::CPU::write8b(m3::TCU::MMIO_ADDR + PRINT_REG * sizeof(reg_t), len);
    // wait until the print was carried out
    while(m3::CPU::read8b(m3::TCU::MMIO_ADDR + PRINT_REG * sizeof(reg_t)) != 0)
        ;
}

void debug_flush(DebugBuf *db) {
#if defined(__host__)
    __syscall3(SYS_write, 1, (long)db->buf, (long)db->pos);
#else
    if(m3::env()->platform == m3::Platform::GEM5) {
        static const char *fileAddr = "stdout";
        gem5_writefile(db->buf, db->pos, 0, reinterpret_cast<uint64_t>(fileAddr));
    }
    else
        print_tcu(db->buf, db->pos);
#endif
    db->pos = 0;
}
