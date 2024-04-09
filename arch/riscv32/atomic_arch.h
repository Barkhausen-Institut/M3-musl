#define a_barrier a_barrier
static inline void a_barrier()
{
	__asm__ __volatile__ ("fence rw,rw" : : : "memory");
}

#define a_cas a_cas
static inline int a_cas(volatile int *p, int t, int s)
{
    int old = *p;
    if(old == t)
        *p = s;
    return old;
}
