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

#include <m3/tiles/OwnActivity.h>

#include <unistd.h>

#include "intern.h"

EXTERN_C int __m3_getpid() {
    // + 1, because our ids start with 0, but pid 0 is special
    return m3::Activity::own().id() + 1;
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

EXTERN_C mode_t __m3_umask(mode_t) {
    // we don't support changes here; just report the typical default value
    return 022;
}
