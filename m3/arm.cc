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

#ifdef __cplusplus
#    define restrict __restrict
#endif

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#undef restrict

// for libstdc++
extern "C" int __fstat_time64(int fd, struct stat *st) {
    return fstat(fd, st);
}
