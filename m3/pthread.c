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

#include "pthread_impl.h"

volatile int __thread_list_lock;

uintptr_t m3_pthread_addr;
struct pthread m3_cur_pthread;

void *__copy_tls(unsigned char *mem) {
    (void)mem;
    return NULL;
}

#if defined(__riscv) && __riscv_xlen == 32
weak int pthread_mutex_init(pthread_mutex_t *, const pthread_mutexattr_t *) {
	return 0;
}
weak int pthread_mutexattr_init(pthread_mutexattr_t *) {
	return 0;
}
weak int pthread_mutexattr_settype(pthread_mutexattr_t *, int) {
	return 0;
}
weak int pthread_mutexattr_destroy(pthread_mutexattr_t *) {
	return 0;
}
#endif

