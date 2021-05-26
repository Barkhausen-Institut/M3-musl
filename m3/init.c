#include "libc.h"
#include "pthread_impl.h"
#include <features.h>
#include <stddef.h>
#include <unistd.h>

static char *posix_env[] = {NULL, NULL};

int __init_tp(void *p) {
    pthread_t td = p;
    td->self = td;
    td->detach_state = DT_JOINABLE;
    td->tid = 0;
    td->locale = &libc.global_locale;
    td->robust_list.head = &td->robust_list.head;
    td->next = td->prev = td;
    return 0;
}

void __m3_init_libc() {
    __init_libc(posix_env, NULL);
}

weak void __init_libc(char **envp, char *pn) {
#if !defined(__host__)
    __environ = envp;
#endif
    libc.auxv = (void *)(envp + 1);
    __progname = __progname_full = "";
    __init_tp(&m3_cur_pthread);
}

hidden void __init_tls(size_t *tls) {
}
