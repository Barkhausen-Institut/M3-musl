#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct DebugBuf {
    char buf[128];
    size_t pos;
} DebugBuf;

void debug_new(DebugBuf *db);
void debug_puts(DebugBuf *db, const char *str);
void debug_putu(DebugBuf *db, unsigned long long n, unsigned int base);
void debug_flush(DebugBuf *db);

#ifdef __cplusplus
}
#endif