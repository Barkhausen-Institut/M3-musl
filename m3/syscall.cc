#include <base/Common.h>
#include <base/log/Lib.h>

extern "C" {
#include <features.h>
#include <bits/syscall.h>
#include <fcntl.h>
#include <errno.h>
}

struct SyscallTraceEntry {
    long number;
    cycles_t start;
    cycles_t end;
};

static SyscallTraceEntry *syscall_trace;
static size_t syscall_trace_pos;
static size_t syscall_trace_size;

static const char *syscall_name(long no) {
    switch(no) {
#if defined(SYS_open)
        case SYS_open:
#endif
        case SYS_openat:            return "open";
        case SYS_read:              return "read";
        case SYS_write:             return "write";
        case SYS_writev:            return "writev";
#if defined(SYS__llseek)
        case SYS__llseek:
#endif
        case SYS_lseek:             return "lseek";
        case SYS_close:             return "close";
#if defined(SYS_fcntl64)
        case SYS_fcntl64:
#endif
        case SYS_fcntl:             return "fcntl";
#if defined(SYS_access)
        case SYS_access:
#endif
        case SYS_faccessat:         return "faccessat";
        case SYS_fsync:             return "fsync";
#if defined(SYS_fstat64)
        case SYS_fstat64:
#endif
        case SYS_fstat:             return "fstat";
#if defined(SYS_stat)
        case SYS_stat:              return "stat";
#endif
#if defined(SYS_stat64)
      case SYS_stat64:              return "stat";
#endif
#if defined(SYS_fstatat)
        case SYS_fstatat:           return "stat";
#endif
        case SYS_getdents64:        return "getdents64";
#if defined(SYS_mkdir)
        case SYS_mkdir:
#endif
        case SYS_mkdirat:           return "mkdirat";
#if defined(SYS_rmdir)
        case SYS_rmdir:             return "rmdir";
#endif
#if defined(SYS_rename)
        case SYS_rename:
#endif
        case SYS_renameat2:         return "rename";
#if defined(SYS_unlink)
        case SYS_unlink:
#endif
        case SYS_unlinkat:          return "unlink";

        case 0xFFFF:                return "receive";
        case 0xFFFE:                return "send";

        default:                    return "<unknown>";
    }
}

EXTERN_C int __m3_openat(int dirfd, const char *pathname, int flags, mode_t mode);
EXTERN_C ssize_t __m3_read(int fd, void *buf, size_t count);
EXTERN_C ssize_t __m3_readv(int fildes, const struct iovec *iov, int iovcnt);
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

EXTERN_C void __m3_sysc_trace(bool enable, size_t max) {
    if(enable) {
        if(!syscall_trace)
            syscall_trace = new SyscallTraceEntry[max]();
        syscall_trace_pos = 0;
        syscall_trace_size = max;
    }
    else {
        char buf[128];
        for(size_t i = 0; i < syscall_trace_pos; ++i) {
            m3::OStringStream os(buf, sizeof(buf));
            os << "[" << m3::fmt(i, 3) << "] " << syscall_name(syscall_trace[i].number)
                                       << " (" << syscall_trace[i].number << ")"
                                       << " " << m3::fmt(syscall_trace[i].start, "0", 11)
                                       << " " << m3::fmt(syscall_trace[i].end, "0", 11)
                                       << "\n";
            m3::Machine::write(os.str(), os.length());
        }

        delete[] syscall_trace;
        syscall_trace = nullptr;
        syscall_trace_pos = 0;
        syscall_trace_size = 0;
    }
}

EXTERN_C void __m3_sysc_trace_start(long n) {
    if(syscall_trace_pos < syscall_trace_size) {
        syscall_trace[syscall_trace_pos].number = n;
        syscall_trace[syscall_trace_pos].start = m3::CPU::elapsed_cycles();
    }
}

EXTERN_C void __m3_sysc_trace_stop() {
    if(syscall_trace_pos < syscall_trace_size) {
        syscall_trace[syscall_trace_pos].end = m3::CPU::elapsed_cycles();
        syscall_trace_pos++;
    }
}

EXTERN_C long __syscall6(long n, long a, long b, long c, long d, long e, long f) {
    long res = -ENOSYS;

    __m3_sysc_trace_start(n);

    switch(n) {
#if defined(SYS_open)
        case SYS_open:              res = __m3_openat(-1, (const char*)a, b, (mode_t)c); break;
#endif
        case SYS_openat:            res = __m3_openat(a, (const char *)b, c, (mode_t)d); break;
        case SYS_read:              res = __m3_read(a, (void*)b, (size_t)c); break;
        case SYS_readv:             res = __m3_readv(a, (const struct iovec*)b, c); break;
        case SYS_write:             res = __m3_write(a, (const void*)b, (size_t)c); break;
        case SYS_writev:            res = __m3_writev(a, (const struct iovec*)b, c); break;
        case SYS_lseek:             res = __m3_lseek(a, b, c); break;
#if defined(SYS__llseek)
        case SYS__llseek: {
            assert(b == 0);
            res = __m3_lseek(a, c, e);
            *(off_t*)d = res;
            break;
        }
#endif
        case SYS_close:             res = __m3_close(a); break;

        case SYS_fcntl:             res = __m3_fcntl(a, b); break;
#if defined(SYS_fcntl64)
        case SYS_fcntl64:           res = __m3_fcntl(a, b); break;
#endif
#if defined(SYS_access)
        case SYS_access:            res = __m3_faccessat(-1, (const char*)a, b, 0); break;
#endif
        case SYS_faccessat:         res = __m3_faccessat(a, (const char*)b, c, d); break;
        case SYS_fsync:             res = __m3_fsync(a); break;

        case SYS_fstat:             res = __m3_fstat(a, (struct kstat*)b); break;
#if defined(SYS_fstat64)
        case SYS_fstat64:           res = __m3_fstat(a, (struct kstat*)b); break;
#endif
#if defined(SYS_stat)
        case SYS_stat:              res = __m3_fstatat(-1, (const char*)a, (struct kstat*)b, 0); break;
#endif
#if defined(SYS_stat64)
      case SYS_stat64:              res = __m3_fstatat(-1, (const char*)a, (struct kstat*)b, 0); break;
#endif
#if defined(SYS_fstatat)
        case SYS_fstatat:           res = __m3_fstatat(a, (const char*)b, (struct kstat*)c, d); break;
#endif
        case SYS_getdents64:        res = __m3_getdents64(a, (void*)b, (size_t)c); break;
#if defined(SYS_mkdir)
        case SYS_mkdir:             res = __m3_mkdirat(-1, (const char*)a, (mode_t)b); break;
#endif
        case SYS_mkdirat:           res = __m3_mkdirat(a, (const char*)b, (mode_t)c); break;
#if defined(SYS_rmdir)
        case SYS_rmdir:             res = __m3_unlinkat(-1, (const char*)a, AT_REMOVEDIR); break;
#endif
#if defined(SYS_rename)
        case SYS_rename:            res = __m3_renameat2(-1, (const char*)a, -1, (const char*)b, 0); break;
#endif
        case SYS_renameat2:         res = __m3_renameat2(a, (const char*)b, c, (const char*)d, (unsigned)e); break;
#if defined(SYS_unlink)
        case SYS_unlink:            res = __m3_unlinkat(-1, (const char*)a, 0); break;
#endif
        case SYS_unlinkat:          res = __m3_unlinkat(a, (const char*)b, c); break;

        // deliberately ignored
        case SYS_ioctl:
        case SYS_prlimit64:
#if defined(SYS_getrlimit)
        case SYS_getrlimit:
#endif
#if defined(SYS_ugetrlimit)
        case SYS_ugetrlimit:
#endif
#if defined(SYS_clock_gettime)
        case SYS_clock_gettime:
#endif
#if defined(SYS_clock_gettime32)
        case SYS_clock_gettime32:
#endif
#if defined(SYS_clock_gettime64)
        case SYS_clock_gettime64:
#endif
#if defined(SYS_gettimeofday)
        case SYS_gettimeofday:
#endif
#if defined(SYS_gettimeofday_time32)
        case SYS_gettimeofday_time32:
#endif
            break;

        default: {
            char buf[128];
            m3::OStringStream os(buf, sizeof(buf));
            os << "unknown syscall(" << n << ", "
                                     << a << ", " << b << ", " << c << ", "
                                     << d << ", " << e << ", " << f
                                     << ")\n";
            m3::Machine::write(os.str(), os.length());
            break;
        }
    }

    __m3_sysc_trace_stop();

    return res;
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
