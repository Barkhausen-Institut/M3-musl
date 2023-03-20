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

#ifdef __cplusplus
#    define restrict __restrict
#endif

#include <base/TMIF.h>

#undef restrict

EXTERN_C void __init_tls_arch(UNUSED uintptr_t addr) {
#if defined(__x86_64__)
    if(m3::TMIF::init_tls(addr) != m3::Errors::SUCCESS)
        abort();
#endif
}
