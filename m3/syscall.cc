#include <base/Common.h>
#include <base/log/Lib.h>

extern "C" {
#include <features.h>
#include <bits/syscall.h>
#include <fcntl.h>
#include <errno.h>
}

EXTERN_C int __m3_openat(int dirfd, const char *pathname, int flags, mode_t mode);
EXTERN_C ssize_t __m3_read(int fd, void *buf, size_t count);
EXTERN_C ssize_t __m3_write(int fd, const void *buf, size_t count);
EXTERN_C ssize_t __m3_writev(int fildes, const struct iovec *iov, int iovcnt);
EXTERN_C int __m3_close(int fd);

EXTERN_C long __syscall6(long n, long a, long b, long c, long d, long e, long f) {
    switch(n) {
#if defined(__x86_64__) || defined(__arm__)
        case SYS_open:      return __m3_openat(-1, (const char*)a, b, static_cast<mode_t>(c));
#endif
        case SYS_openat:    return __m3_openat(a, (const char *)b, c, static_cast<mode_t>(d));
        case SYS_read:      return __m3_read(a, (void*)b, (size_t)c);
        case SYS_write:     return __m3_write(a, (const void*)b, (size_t)c);
        case SYS_writev:    return __m3_writev(a, (const struct iovec*)b, c);
        case SYS_close:     return __m3_close(a);

        // deliberately ignored
        case SYS_ioctl:     return -ENOSYS;

        default: {
            char buf[128];
            m3::OStringStream os(buf, sizeof(buf));
            os << "unknown syscall(" << n << ", "
                                     << a << ", " << b << ", " << c << ", "
                                     << d << ", " << e << ", " << f
                                     << ")\n";
            m3::Machine::write(os.str(), os.length());
            return -ENOSYS;
        }
    }
}

EXTERN_C long __syscall_cp(long n, long a, long b, long c, long d, long e, long f) {
    return __syscall6(n, a, b, c, d, e, f);
}

EXTERN_C long __syscall0(long n) {
    return __syscall6(n, 0, 0, 0, 0, 0, 0);
}
EXTERN_C long __syscall1(long n, long a) {
    return __syscall6(n, a, 0, 0, 0, 0, 0);
}
EXTERN_C long __syscall2(long n, long a, long b) {
    return __syscall6(n, a, b, 0, 0, 0, 0);
}
EXTERN_C long __syscall3(long n, long a, long b, long c) {
    return __syscall6(n, a, b, c, 0, 0, 0);
}
EXTERN_C long __syscall4(long n, long a, long b, long c, long d) {
    return __syscall6(n, a, b, c, d, 0, 0);
}
EXTERN_C long __syscall5(long n, long a, long b, long c, long d, long e) {
    return __syscall6(n, a, b, c, d, e, 0);
}
