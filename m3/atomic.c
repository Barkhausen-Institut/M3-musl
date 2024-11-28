/*
 * Copyright (C) 2024 Nils Asmussen, Barkhausen Institut
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

#include <stdbool.h>
#include <stdint.h>

#if defined(__riscv) && __riscv_xlen == 32
#define ATOMIC_LOAD(T, n)                                         \
    T __atomic_load_##n(const volatile void *ptr, int memorder) { \
        (void)memorder;                                           \
        return *(const volatile T*)ptr;                           \
    }

#define ATOMIC_STORE(T, n)                                             \
    void __atomic_store_##n(volatile void *ptr, T val, int memorder) { \
        (void)memorder;                                                \
        *(volatile T*)ptr = val;                                       \
    }

#define ATOMIC_EXCHANGE(T, n)                                              \
    T __atomic_exchange_##n(volatile void *ptr, T desired, int memorder) { \
        (void) memorder;                                                   \
        T old = *(volatile T*)ptr;                                         \
        *(volatile T*)ptr = desired;                                       \
        return old;                                                        \
    }

#define ATOMIC_FETCH_OP(opname, op, T, n)                                      \
    T __atomic_fetch_##opname##_##n(volatile void *ptr, T val, int memmodel) { \
        (void)memmodel;                                                        \
        T tmp = *(volatile T*)ptr;                                             \
        *(volatile T*)ptr = tmp op val;                                        \
        return tmp;                                                            \
    }

#define ATOMIC_CMPXCHG(T, n)                                                 \
    bool __atomic_compare_exchange_##n(volatile void *ptr, void *expected,   \
                                       T desired, bool weak,                 \
                                       int success_memorder,                 \
                                       int failure_memorder) {               \
        (void)weak;                                                          \
        (void)success_memorder;                                              \
        (void)failure_memorder;                                              \
        T cur = *(volatile T*)ptr;                                           \
        if (cur != *(T*)expected) {                                          \
            *(T*)expected = cur;                                             \
            return false;                                                    \
        }                                                                    \
        *(volatile T*)ptr = desired;                                         \
        return true;                                                         \
    }

ATOMIC_LOAD(uint8_t, 1)
ATOMIC_LOAD(uint32_t, 4)
ATOMIC_STORE(uint8_t, 1)
ATOMIC_STORE(uint32_t, 4)
ATOMIC_EXCHANGE(uint8_t, 1)
ATOMIC_EXCHANGE(uint32_t, 4)
ATOMIC_CMPXCHG(uint8_t, 1)
ATOMIC_CMPXCHG(uint32_t, 4)
ATOMIC_FETCH_OP(and, &, uint8_t, 1)
ATOMIC_FETCH_OP(or, |, uint8_t, 1)
ATOMIC_FETCH_OP(add, +, uint32_t, 4)
ATOMIC_FETCH_OP(sub, -, uint32_t, 4)
#endif
