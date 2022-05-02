#if defined(__host__)
#   include_next "pthread_arch.h"
#else

extern uintptr_t m3_pthread_addr;

static inline uintptr_t __get_tp()
{
    return (uintptr_t)m3_pthread_addr;
}

#if defined(__x86_64__)
#   define MC_PC gregs[REG_RIP]
#elif defined(__riscv)
#   define GAP_ABOVE_TP 0
#   define DTP_OFFSET 0x800
#   define MC_PC __gregs[0]
#else
#   define GAP_ABOVE_TP 8
#   define MC_PC arm_pc
#endif

#endif
