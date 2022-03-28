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

#include <m3/tiles/Activity.h>

#define _GNU_SOURCE
#include <unistd.h>

#include "intern.h"

EXTERN_C int __m3_getpid() {
    return m3::Activity::self().id();
}

// pretend that we're running as root
EXTERN_C int __m3_getuid() {
    return 0;
}
EXTERN_C int __m3_geteuid() {
    return 0;
}
EXTERN_C int __m3_getgid() {
    return 0;
}
EXTERN_C int __m3_getegid() {
    return 0;
}
