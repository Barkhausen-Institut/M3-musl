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

#include <base/Common.h>
#include <base/log/Lib.h>
#include <base/time/Instant.h>

extern "C" {
#include <features.h>
#include <bits/syscall.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
}

#include "intern.h"

#define PRINT_SYSCALLS 0

struct SyscallTraceEntry {
    explicit SyscallTraceEntry() : number(), start(m3::TimeInstant::now()), end(m3::TimeInstant::now()) {
    }

    long number;
    m3::TimeInstant start;
    m3::TimeInstant end;
};

static SyscallTraceEntry *syscall_trace;
static size_t syscall_trace_pos;
static size_t syscall_trace_size;
static m3::TimeDuration system_time;

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
        case SYS_ftruncate:         return "ftruncate";
        case SYS_truncate:          return "truncate";
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
#if defined(SYS_lstat)
        case SYS_lstat:             return "lstat";
#endif
        case SYS_getdents64:        return "getdents64";
#if defined(SYS_mkdir)
        case SYS_mkdir:
#endif
        case SYS_mkdirat:           return "mkdir";
#if defined(SYS_rmdir)
        case SYS_rmdir:             return "rmdir";
#endif
#if defined(SYS_rename)
        case SYS_rename:
#endif
        case SYS_renameat2:         return "rename";
#if defined(SYS_link)
        case SYS_link:
#endif
        case SYS_linkat:            return "link";
#if defined(SYS_unlink)
        case SYS_unlink:
#endif
        case SYS_unlinkat:          return "unlink";

        case SYS_socket:            return "socket";
        case SYS_connect:           return "connect";
        case SYS_bind:              return "bind";
        case SYS_listen:            return "listen";
        case SYS_accept:            return "accept";
        case SYS_accept4:           return "accept4";
        case SYS_sendto:            return "sendto";
        case SYS_sendmsg:           return "sendmsg";
        case SYS_recvfrom:          return "recvfrom";
        case SYS_recvmsg:           return "recvmsg";
        case SYS_shutdown:          return "shutdown";
        case SYS_getsockname:       return "getsockname";
        case SYS_getpeername:       return "getpeername";

        case SYS_getpid:            return "getpid";
        case SYS_getuid:            return "getuid";
#if defined(SYS_getuid32)
        case SYS_getuid32:          return "getuid";
#endif
        case SYS_geteuid:           return "geteuid";
#if defined(SYS_geteuid32)
        case SYS_geteuid32:         return "geteuid";
#endif
        case SYS_getgid:            return "getgid";
#if defined(SYS_getgid32)
        case SYS_getgid32:          return "getgid";
#endif
        case SYS_getegid:           return "getegid";
#if defined(SYS_getegid32)
        case SYS_getegid32:         return "getegid";
#endif
        case SYS_umask:             return "umask";

#if defined(SYS_clock_gettime)
        case SYS_clock_gettime:     return "clock_gettime";
#endif
#if defined(SYS_clock_gettime32)
        case SYS_clock_gettime32:   return "clock_gettime";
#endif
#if defined(SYS_clock_gettime64)
        case SYS_clock_gettime64:   return "clock_gettime";
#endif

        case 0xFFFF:                return "receive";
        case 0xFFFE:                return "send";

        default: {
            static char tmp[32];
            m3::OStringStream os(tmp, sizeof(tmp));
            os << "unknown[" << no << "]";
            return tmp;
        }
    }
}

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
        case m3::Errors::BAD_FD: return EBADF;
        default: return ENOSYS;
    }
}

EXTERN_C void __m3_sysc_trace(bool enable, size_t max) {
    if(enable) {
        if(!syscall_trace)
            syscall_trace = new SyscallTraceEntry[max]();
        syscall_trace_pos = 0;
        syscall_trace_size = max;
        system_time = m3::TimeDuration::ZERO;
    }
    else {
        char buf[128];
        for(size_t i = 0; i < syscall_trace_pos; ++i) {
            m3::OStringStream os(buf, sizeof(buf));
            os << "[" << m3::fmt(i, 3) << "] " << syscall_name(syscall_trace[i].number)
                                       << " (" << syscall_trace[i].number << ")"
                                       << " " << m3::fmt(syscall_trace[i].start.as_nanos(), "0", 11)
                                       << " " << m3::fmt(syscall_trace[i].end.as_nanos(), "0", 11)
                                       << "\n";
            m3::Machine::write(os.str(), os.length());
        }

        delete[] syscall_trace;
        syscall_trace = nullptr;
        syscall_trace_pos = 0;
        syscall_trace_size = 0;
        system_time = m3::TimeDuration::ZERO;
    }
}

EXTERN_C uint64_t __m3_sysc_systime() {
    return system_time.as_nanos();
}

EXTERN_C void __m3_sysc_trace_start(long n) {
    if(syscall_trace_pos < syscall_trace_size) {
        syscall_trace[syscall_trace_pos].number = n;
        syscall_trace[syscall_trace_pos].start = m3::TimeInstant::now();
    }
}

EXTERN_C void __m3_sysc_trace_stop() {
    if(syscall_trace_pos < syscall_trace_size) {
        syscall_trace[syscall_trace_pos].end = m3::TimeInstant::now();
        syscall_trace_pos++;
        system_time += syscall_trace[syscall_trace_pos].end.duration_since(syscall_trace[syscall_trace_pos].start);
    }
}

void print_syscall(m3::OStringStream &os, long n, long a, long b, long c, long d, long e, long f) {
    os << syscall_name(n) << "("
                          << a << ", " << b << ", " << c << ", "
                          << d << ", " << e << ", " << f
                          << ")";
}

EXTERN_C long __syscall6(long n, long a, long b, long c, long d, long e, long f) {
    long res = -ENOSYS;

    __m3_sysc_trace_start(n);

#if PRINT_SYSCALLS
    char syscbuf[256];
    m3::OStringStream syscos(syscbuf, sizeof(syscbuf));
    print_syscall(syscos, n, a, b, c, d, e, f);
    syscos << " ...\n";
    m3::Machine::write(syscos.str(), syscos.length());
#endif

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
#if defined(SYS_lstat)
        case SYS_lstat:             res = __m3_fstatat(-1, (const char*)a, (struct kstat*)b, 0); break;
#endif
#if defined(SYS_stat64)
      case SYS_stat64:              res = __m3_fstatat(-1, (const char*)a, (struct kstat*)b, 0); break;
#endif
#if defined(SYS_fstatat)
        case SYS_fstatat:           res = __m3_fstatat(a, (const char*)b, (struct kstat*)c, d); break;
#endif
        case SYS_ftruncate:         res = __m3_ftruncate(a, b); break;
        case SYS_truncate:          res = __m3_truncate((const char*)a, b); break;
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
#if defined(SYS_link)
        case SYS_link:              res = __m3_linkat(-1, (const char*)a, -1, (const char*)b, 0); break;
#endif
        case SYS_linkat:            res = __m3_linkat(a, (const char*)b, c, (const char*)d, e); break;
#if defined(SYS_unlink)
        case SYS_unlink:            res = __m3_unlinkat(-1, (const char*)a, 0); break;
#endif
        case SYS_unlinkat:          res = __m3_unlinkat(a, (const char*)b, c); break;

        case SYS_socket:            res = __m3_socket(a, b, c); break;
        case SYS_bind:              res = __m3_bind(a, (const struct sockaddr*)b, (socklen_t)c); break;
        case SYS_listen:            res = __m3_listen(a, b); break;
        case SYS_accept:            res = __m3_accept(a, (struct sockaddr*)b, (socklen_t*)c); break;
        case SYS_accept4:           res = __m3_accept4(a, (struct sockaddr*)b, (socklen_t*)c, d); break;
        case SYS_connect:           res = __m3_connect(a, (const struct sockaddr*)b, (socklen_t)c); break;
        case SYS_sendto:            res = __m3_sendto(a, (const void*)b, (size_t)c, d, (const struct sockaddr*)e, (socklen_t)f); break;
        case SYS_sendmsg:           res = __m3_sendmsg(a, (const struct msghdr*)b, c); break;
        case SYS_recvfrom:          res = __m3_recvfrom(a, (void*)b, (size_t)c, d, (struct sockaddr*)e, (socklen_t*)f); break;
        case SYS_recvmsg:           res = __m3_recvmsg(a, (struct msghdr*)b, c); break;
        case SYS_shutdown:          res = __m3_shutdown(a, b); break;
        case SYS_getsockname:       res = __m3_getsockname(a, (struct sockaddr*)b, (socklen_t*)c); break;
        case SYS_getpeername:       res = __m3_getpeername(a, (struct sockaddr*)b, (socklen_t*)c); break;

        case SYS_getpid:            res = __m3_getpid(); break;
        case SYS_getuid:            res = __m3_getuid(); break;
#if defined(SYS_getuid32)
        case SYS_getuid32:          res = __m3_getuid(); break;
#endif
        case SYS_geteuid:           res = __m3_geteuid(); break;
#if defined(SYS_geteuid32)
        case SYS_geteuid32:         res = __m3_geteuid(); break;
#endif
        case SYS_getgid:            res = __m3_getgid(); break;
#if defined(SYS_getgid32)
        case SYS_getgid32:          res = __m3_getgid(); break;
#endif
        case SYS_getegid:           res = __m3_getegid(); break;
#if defined(SYS_getegid32)
        case SYS_getegid32:         res = __m3_getegid(); break;
#endif
        case SYS_umask:             res = (long)__m3_umask((mode_t)a); break;

#if defined(SYS_clock_gettime)
        case SYS_clock_gettime:     res = __m3_clock_gettime(a, (struct timespec*)b); break;
#endif
#if defined(SYS_clock_gettime32)
        case SYS_clock_gettime32:   res = __m3_clock_gettime(a, (struct timespec*)b); break;
#endif
#if defined(SYS_clock_gettime64)
        case SYS_clock_gettime64:   res = __m3_clock_gettime(a, (struct timespec*)b); break;
#endif

        // deliberately ignored
        case SYS_ioctl:
        case SYS_prlimit64:
#if defined(SYS_getrlimit)
        case SYS_getrlimit:
#endif
#if defined(SYS_ugetrlimit)
        case SYS_ugetrlimit:
#endif
            break;

        default: {
#if !PRINT_SYSCALLS
            char buf[256];
            m3::OStringStream os(buf, sizeof(buf));
            print_syscall(os, n, a, b, c, d, e, f);
            os << " -> " << res << "\n";
            m3::Machine::write(os.str(), os.length());
#endif
            break;
        }
    }

#if PRINT_SYSCALLS
    syscos.reset();
    print_syscall(syscos, n, a, b, c, d, e, f);
    syscos << " -> " << res << "\n";
    m3::Machine::write(syscos.str(), syscos.length());
#endif

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
