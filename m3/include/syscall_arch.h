#define __SYSCALL_LL_E(x) (x)
#define __SYSCALL_LL_O(x) (x)

long __syscall0(long n);
long __syscall1(long n, long a);
long __syscall2(long n, long a, long b);
long __syscall3(long n, long a, long b, long c);
long __syscall4(long n, long a, long b, long c, long d);
long __syscall5(long n, long a, long b, long c, long d, long e);
long __syscall6(long n, long a, long b, long c, long d, long e, long f);

#define VDSO_USEFUL
/* We don't have a clock_gettime function.
#define VDSO_CGT_SYM "__vdso_clock_gettime"
#define VDSO_CGT_VER "LINUX_2.6" */

#define IPC_64 0
