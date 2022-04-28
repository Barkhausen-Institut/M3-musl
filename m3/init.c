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

#include "libc.h"
#include "pthread_impl.h"
#include <features.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

extern struct pthread m3_cur_pthread;

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

void __m3_init_libc(int argc, char **argv, char **envp) {
    __progname_full = argv[0];
    __progname = __progname_full;
    char *last_slash = strrchr(__progname, '/');
    if(last_slash)
        __progname = last_slash + 1;

    __init_libc(envp, NULL);
}

weak void __init_libc(char **envp, char *pn) {
#if !defined(__host__)
    __environ = envp;
#endif
    libc.auxv = (void *)(envp + 1);
    __init_tp(&m3_cur_pthread);
    m3_pthread_addr = (uintptr_t)&m3_cur_pthread;
}

hidden void __init_tls(size_t *tls) {
}
