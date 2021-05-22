#include "../src/internal/libc.h"
#include <features.h>
#include <stddef.h>
#include <unistd.h>

static char *posix_env[] = {NULL, NULL};

void __m3_init_libc() {
    __init_libc(posix_env, NULL);
}

weak void __init_libc(char **envp, char *pn) {
#if !defined(__host__)
    __environ = envp;
#endif
    libc.auxv = (void *)(envp + 1);
    __progname = __progname_full = "";
}

hidden void __init_tls(size_t *tls) {
}
