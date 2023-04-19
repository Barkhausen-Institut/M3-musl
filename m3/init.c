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

#include <features.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include "libc.h"
#include "pthread_impl.h"

static struct builtin_tls {
    void *space[16];
} builtin_tls[1];

extern struct pthread m3_cur_pthread;

extern void *_tdata_start;
extern void *_tdata_end;

extern void __init_tls_arch(uintptr_t addr);

static int tls_enabled = 0;
static char *null_ptr[] = {NULL};

int __init_tp(void *p) {
    pthread_t td = p;
    td->self = td;
    td->detach_state = DT_JOINABLE;
    td->tid = 0;
    td->locale = &libc.global_locale;
    td->robust_list.head = &td->robust_list.head;
    td->next = td->prev = td;
    __init_tls(NULL);
    return 0;
}

void __m3_init_libc(int argc, char **argv, char **envp, int tls) {
    tls_enabled = tls;
    __progname_full = argv ? argv[0] : "";
    __progname = __progname_full;
    char *last_slash = strrchr(__progname, '/');
    if(last_slash)
        __progname = last_slash + 1;

    __init_libc(envp ? envp : null_ptr, NULL);
}

void __m3_set_args(char **argv, char **envp) {
    __progname_full = argv[0];
    __progname = __progname_full;
    __environ = envp;
}

weak void __init_libc(char **envp, char *pn) {
    __environ = envp;
    libc.auxv = (size_t *)null_ptr;
    __init_tp(&m3_cur_pthread);
    m3_pthread_addr = (uintptr_t)&m3_cur_pthread;
}

hidden void __init_tls(size_t *s) {
    pthread_t td = &m3_cur_pthread;
    td->dtv = (uintptr_t *)builtin_tls;
    size_t tls_size = tls_enabled ? &_tdata_end - &_tdata_start : 0;
    if(tls_size > 0) {
        if(tls_size > sizeof(builtin_tls) - sizeof(uintptr_t) * 2)
            abort();
        uintptr_t tls_dest = (uintptr_t)(td->dtv + 2);
        td->dtv[0] = 1;
        td->dtv[1] = tls_dest + DTP_OFFSET;
        memcpy((void *)tls_dest, &_tdata_start, tls_size);
        __init_tls_arch((uintptr_t)td);
    }
    else
        td->dtv[0] = 0;
}
