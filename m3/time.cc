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
#include <base/time/Duration.h>

#include <m3/tiles/OwnActivity.h>

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

EXTERN_C int __m3_nanosleep(const struct timespec *req, struct timespec *rem) {
    auto start = m3::TimeInstant::now();

    uint64_t nanos = static_cast<uint64_t>(req->tv_nsec) + static_cast<ulong>(req->tv_sec) * 1000000000;
    m3::Activity::sleep_for(m3::TimeDuration::from_nanos(nanos));

    if(rem) {
        auto duration = m3::TimeInstant::now().duration_since(start);
        auto remaining = m3::TimeDuration::from_nanos(nanos) - duration;
        rem->tv_sec = static_cast<time_t>(remaining.as_secs());
        rem->tv_nsec = static_cast<long>(remaining.as_nanos() - (static_cast<ulong>(rem->tv_sec) * 1000000000));
    }
    return 0;
}
