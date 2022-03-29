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
#include <base/time/Instant.h>

#define __NEED_struct_timeval
#define __NEED_time_t
#define __NEED_suseconds_t

#define _GNU_SOURCE
#include <errno.h>
#include <unistd.h>

#include "intern.h"

EXTERN_C int __m3_clock_gettime(clockid_t clockid, struct timespec *tp) {
    if(clockid != CLOCK_REALTIME && clockid != CLOCK_MONOTONIC)
        return -ENOTSUP;

    auto now = m3::TimeInstant::now().elapsed();
    tp->tv_sec = static_cast<time_t>(now.as_secs());
    tp->tv_nsec = static_cast<long>(now.as_nanos() - (static_cast<ulong>(tp->tv_sec) * 1000000000));
    return 0;
}
