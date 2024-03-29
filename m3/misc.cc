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

#define _GNU_SOURCE // for domainname
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "intern.h"

EXTERN_C int __m3_uname(struct utsname *buf) {
    strcpy(buf->sysname, "M3");
    strcpy(buf->nodename, "kachel");
    strcpy(buf->release, "latest");
    strcpy(buf->version, "0.1");
#if defined(__riscv)
    strcpy(buf->machine, "RISC-V");
#elif defined(__x86_64__)
    strcpy(buf->machine, "x86-64");
#elif defined(__arm__)
    strcpy(buf->machine, "ARMv7");
#else
#    error "Unsupported ISA"
#endif
    strcpy(buf->domainname, "localhost");
    return 0;
}

EXTERN_C int __m3_ioctl(int fd, unsigned long request, ...) {
    // we only support the TIOCGWINSZ request so that isatty() works
    if(request != TIOCGWINSZ)
        return -ENOSYS;
    if(!__m3c_isatty(fd))
        return -ENOTSUP;

    va_list ap;
    va_start(ap, request);
    struct winsize *ws = va_arg(ap, struct winsize *);
    ws->ws_row = 25;
    ws->ws_col = 100;
    ws->ws_xpixel = 0;
    ws->ws_ypixel = 0;
    va_end(ap);
    return 0;
}
