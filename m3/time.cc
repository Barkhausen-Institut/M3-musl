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

#define __NEED_struct_timeval
#define __NEED_time_t
#define __NEED_suseconds_t

#include <errno.h>
#include <unistd.h>

#include "intern.h"

EXTERN_C int __m3_clock_gettime(clockid_t clockid, struct timespec *tp) {
    if(clockid != CLOCK_REALTIME && clockid != CLOCK_MONOTONIC)
        return -ENOTSUP;

    int seconds;
    long nanos;
    __m3c_get_time(&seconds, &nanos);
    tp->tv_sec = seconds;
    tp->tv_nsec = nanos;
    return 0;
}

EXTERN_C int __m3_nanosleep(const struct timespec *req, struct timespec *rem) {
    int seconds = req->tv_sec;
    long nanos = req->tv_nsec;
    __m3c_sleep(&seconds, &nanos);

    if(rem) {
        rem->tv_sec = seconds;
        rem->tv_nsec = nanos;
    }
    return 0;
}
