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

uintptr_t m3_pthread_addr;
struct pthread m3_cur_pthread;

uintptr_t __p2m3_m3_pthread_addr __attribute__((__alias__("m3_pthread_addr")));
struct pthread __p2m3_m3_cur_pthread __attribute__((__alias__("m3_cur_pthread")));
