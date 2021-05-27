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
EXTERN_C off_t __m3_lseek(int fd, off_t offset, int whence);
EXTERN_C int __m3_close(int fd);

EXTERN_C int __m3_fcntl(int fd, int cmd, ... /* arg */ );
EXTERN_C int __m3_faccessat(int dirfd, const char *pathname, int mode, int flags);
EXTERN_C int __m3_fsync(int fd);

EXTERN_C int __m3_fstat(int fd, struct kstat *statbuf);
EXTERN_C int __m3_fstatat(int dirfd, const char *pathname,
                          struct kstat *statbuf, int flags);
EXTERN_C ssize_t __m3_getdents64(int fd, void *dirp, size_t count);
EXTERN_C int __m3_mkdirat(int dirfd, const char *pathname, mode_t mode);
EXTERN_C int __m3_renameat2(int olddirfd, const char *oldpath, int newdirfd,
                            const char *newpath, unsigned int flags);
EXTERN_C int __m3_unlinkat(int dirfd, const char *pathname, int flags);

EXTERN_C int __m3_posix_errno(int m3_error) {
    switch(m3_error) {
        case m3::Errors::NONE: return 0;
        case m3::Errors::INV_ARGS: return EINVAL;
        case m3::Errors::OUT_OF_MEM: return ENOMEM;
        case m3::Errors::NO_SUCH_FILE: return ENOENT;
        case m3::Errors::NOT_SUP: return ENOTSUP;
        case m3::Errors::NO_SPACE: return ENOSPC;
        case m3::Errors::EXISTS: return EEXIST;
        case m3::Errors::XFS_LINK: return EXDEV;
        case m3::Errors::DIR_NOT_EMPTY: return ENOTEMPTY;
        case m3::Errors::IS_DIR: return EISDIR;
        case m3::Errors::IS_NO_DIR: return ENOTDIR;
        case m3::Errors::TIMEOUT: return ETIMEDOUT;
        case m3::Errors::NO_PERM: return EPERM;
        default: return ENOSYS;
    }
}

EXTERN_C long __syscall6(long n, long a, long b, long c, long d, long e, long f) {
    switch(n) {
#if defined(SYS_open)
        case SYS_open:              return __m3_openat(-1, (const char*)a, b, (mode_t)c);
#endif
        case SYS_openat:            return __m3_openat(a, (const char *)b, c, (mode_t)d);
        case SYS_read:              return __m3_read(a, (void*)b, (size_t)c);
        case SYS_write:             return __m3_write(a, (const void*)b, (size_t)c);
        case SYS_writev:            return __m3_writev(a, (const struct iovec*)b, c);
        case SYS_lseek:             return __m3_lseek(a, b, c);
#if defined(SYS__llseek)
        case SYS__llseek: {
            assert(b == 0);
            long res = __m3_lseek(a, c, e);
            *(off_t*)d = res;
            return res;
        }
#endif
        case SYS_close:             return __m3_close(a);

        case SYS_fcntl:             return __m3_fcntl(a, b);
#if defined(SYS_fcntl64)
        case SYS_fcntl64:           return __m3_fcntl(a, b);
#endif
#if defined(SYS_access)
        case SYS_access:            return __m3_faccessat(-1, (const char*)a, b, 0);
#endif
        case SYS_faccessat:         return __m3_faccessat(a, (const char*)b, c, d);
        case SYS_fsync:             return __m3_fsync(a);

        case SYS_fstat:             return __m3_fstat(a, (struct kstat*)b);
#if defined(SYS_fstat64)
        case SYS_fstat64:           return __m3_fstat(a, (struct kstat*)b);
#endif
#if defined(SYS_stat)
        case SYS_stat:              return __m3_fstatat(-1, (const char*)a, (struct kstat*)b, 0);
#endif
#if defined(SYS_stat64)
      case SYS_stat64:              return __m3_fstatat(-1, (const char*)a, (struct kstat*)b, 0);
#endif
#if defined(SYS_fstatat)
        case SYS_fstatat:           return __m3_fstatat(a, (const char*)b, (struct kstat*)c, d);
#endif
        case SYS_getdents64:        return __m3_getdents64(a, (void*)b, (size_t)c);
#if defined(SYS_mkdir)
        case SYS_mkdir:             return __m3_mkdirat(-1, (const char*)a, (mode_t)b);
#endif
        case SYS_mkdirat:           return __m3_mkdirat(a, (const char*)b, (mode_t)c);
#if defined(SYS_rmdir)
        case SYS_rmdir:             return __m3_unlinkat(-1, (const char*)a, AT_REMOVEDIR);
#endif
#if defined(SYS_rename)
        case SYS_rename:            return __m3_renameat2(-1, (const char*)a, -1, (const char*)b, 0);
#endif
        case SYS_renameat2:         return __m3_renameat2(a, (const char*)b, c, (const char*)d, (unsigned)e);
#if defined(SYS_unlink)
        case SYS_unlink:            return __m3_unlinkat(-1, (const char*)a, 0);
#endif
        case SYS_unlinkat:          return __m3_unlinkat(a, (const char*)b, c);

        // deliberately ignored
        case SYS_ioctl:             return -ENOSYS;
        case SYS_prlimit64:         return -ENOSYS;
#if defined(SYS_getrlimit)
        case SYS_getrlimit:         return -ENOSYS;
#endif
#if defined(SYS_ugetrlimit)
        case SYS_ugetrlimit:        return -ENOSYS;
#endif
#if defined(SYS_clock_gettime)
        case SYS_clock_gettime:     return -ENOSYS;
#endif
#if defined(SYS_clock_gettime32)
        case SYS_clock_gettime32:   return -ENOSYS;
#endif
#if defined(SYS_clock_gettime64)
        case SYS_clock_gettime64:   return -ENOSYS;
#endif
#if defined(SYS_gettimeofday)
        case SYS_gettimeofday:      return -ENOSYS;
#endif
#if defined(SYS_gettimeofday_time32)
        case SYS_gettimeofday_time32: return -ENOSYS;
#endif

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
