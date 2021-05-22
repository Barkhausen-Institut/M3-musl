#include <base/Common.h>
#if !defined(__host__)
#   include <base/Env.h>
#endif

#include <debug.h>

#if defined(__host__)
#   include "../arch/x86_64/syscall_arch.h"
#   include <bits/syscall.h>
#else
EXTERN_C void gem5_writefile(const char *str, uint64_t len, uint64_t offset, uint64_t file);
#endif

void debug_new(DebugBuf *db) {
    db->pos = 0;
#if !defined(__host__)
    debug_puts(db, "[        @");
    debug_putu(db, m3::env()->pe_id, 16);
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
#if defined(__host__)
    __syscall3(SYS_write, 1, (long)db->buf, (long)db->pos);
#else
    static const char *fileAddr = "stdout";
    gem5_writefile(db->buf, db->pos, 0, reinterpret_cast<uint64_t>(fileAddr));
#endif
    db->pos = 0;
}
