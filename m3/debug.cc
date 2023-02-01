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

#include <m3/Compat.h>

#include <debug.h>

EXTERN_C void gem5_writefile(const char *str, uint64_t len, uint64_t offset, uint64_t file);

void debug_new(DebugBuf *db) {
    db->pos = 0;
    debug_puts(db, "[musl       @");
    debug_putu(db, m3::env()->tile_id, 16);
    debug_puts(db, "] ");
}

void debug_putc(DebugBuf *db, char c) {
    if(db->pos < sizeof(db->buf))
        db->buf[db->pos++] = c;
}

void debug_puts(DebugBuf *db, const char *str) {
    char c;
    while((c = *str++))
        debug_putc(db, c);
}

size_t debug_putu_rec(char *buf, size_t space, ullong n, uint base) {
    static char hexchars_small[] = "0123456789abcdef";
    size_t res = 0;
    if(n >= base)
        res += debug_putu_rec(buf, space, n / base, base);
    if(res < space)
        buf[res] = hexchars_small[n % base];
    return res + 1;
}

void debug_putu(DebugBuf *db, ullong n, uint base) {
    db->pos += debug_putu_rec(db->buf + db->pos, sizeof(db->buf) - db->pos, n, base);
}

// put in kernel::TCU to get access to the private members of the TCU class
namespace kernel {
class TCU {
public:
    static void print_tcu(const char *str, size_t len) {
        typedef uint64_t reg_t;

        len = m3::Math::min(len, m3::TCU::PRINT_REGS * sizeof(reg_t) - 1);

        uintptr_t buffer =
            m3::TCU::MMIO_ADDR +
            (m3::TCU::EXT_REGS + m3::TCU::UNPRIV_REGS + TOTAL_EPS * 3) * sizeof(reg_t);
        const reg_t *rstr = reinterpret_cast<const reg_t *>(str);
        const reg_t *end = reinterpret_cast<const reg_t *>(str + len);
        while(rstr < end) {
            m3::CPU::write8b(buffer, *rstr);
            buffer += sizeof(reg_t);
            rstr++;
        }

        auto PRINT_REG = static_cast<size_t>(m3::TCU::UnprivRegs::PRINT);
        m3::CPU::write8b(m3::TCU::MMIO_ADDR + PRINT_REG * sizeof(reg_t), len);
        // wait until the print was carried out
        while(m3::CPU::read8b(m3::TCU::MMIO_ADDR + PRINT_REG * sizeof(reg_t)) != 0)
            ;
    }
};
}

void debug_flush(DebugBuf *db) {
    if(m3::env()->platform == m3::Platform::GEM5) {
        static const char *fileAddr = "stdout";
        gem5_writefile(db->buf, db->pos, 0, reinterpret_cast<uint64_t>(fileAddr));
    }
    kernel::TCU::print_tcu(db->buf, db->pos);
    db->pos = 0;
}
