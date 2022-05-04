/*
 * Copyright (C) 2021-2022 Nils Asmussen, Barkhausen Institut
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

#pragma once

#include <base/Common.h>

// "restrict" is a keyword in C, but not in C++. Thus, redefine it for the inclusion of time.h
// to the __restrict keyword of C++.
#ifdef __cplusplus
#    define restrict __restrict
#endif

#include <fcntl.h>
#include <sys/socket.h>
#include <time.h>

#undef restrict

EXTERN_C int __m3_posix_errno(int m3_error);

// file syscalls
EXTERN_C int __m3_openat(int dirfd, const char *pathname, int flags, mode_t mode);
EXTERN_C ssize_t __m3_read(int fd, void *buf, size_t count);
EXTERN_C ssize_t __m3_readv(int fildes, const struct iovec *iov, int iovcnt);
EXTERN_C ssize_t __m3_write(int fd, const void *buf, size_t count);
EXTERN_C ssize_t __m3_writev(int fildes, const struct iovec *iov, int iovcnt);
EXTERN_C int __m3_fflush(int fd);
EXTERN_C off_t __m3_lseek(int fd, off_t offset, int whence);
EXTERN_C int __m3_ftruncate(int fd, off_t length);
EXTERN_C int __m3_truncate(const char *pathname, off_t length);
EXTERN_C int __m3_close(int fd);
EXTERN_C int __m3_fcntl(int fd, int cmd, ... /* arg */);
EXTERN_C int __m3_faccessat(int dirfd, const char *pathname, int mode, int flags);
EXTERN_C int __m3_fsync(int fd);

// directory syscalls
EXTERN_C int __m3_fstat(int fd, struct kstat *statbuf);
EXTERN_C int __m3_fstatat(int dirfd, const char *pathname, struct kstat *statbuf, int flags);
EXTERN_C ssize_t __m3_getdents64(int fd, void *dirp, size_t count);
EXTERN_C int __m3_mkdirat(int dirfd, const char *pathname, mode_t mode);
EXTERN_C int __m3_renameat2(int olddirfd, const char *oldpath, int newdirfd, const char *newpath,
                            unsigned int flags);
EXTERN_C int __m3_linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath,
                         int flags);
EXTERN_C int __m3_unlinkat(int dirfd, const char *pathname, int flags);
EXTERN_C void __m3_closedir(int fd);
EXTERN_C int __m3_chdir(const char *path);
EXTERN_C int __m3_fchdir(int fd);

// socket syscalls
EXTERN_C int __m3_socket(int domain, int type, int protocol);
EXTERN_C int __m3_getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
EXTERN_C int __m3_getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
EXTERN_C int __m3_bind(int fd, const struct sockaddr *addr, socklen_t addrlen);
EXTERN_C int __m3_listen(int sockfd, int backlog);
EXTERN_C int __m3_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
EXTERN_C int __m3_accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags);
EXTERN_C int __m3_connect(int fd, const struct sockaddr *addr, socklen_t addrlen);
EXTERN_C ssize_t __m3_sendto(int sockfd, const void *buf, size_t len, int flags,
                             const struct sockaddr *dest_addr, socklen_t addrlen);
EXTERN_C ssize_t __m3_sendmsg(int sockfd, const struct msghdr *msg, int flags);
EXTERN_C ssize_t __m3_recvfrom(int sockfd, void *buf, size_t len, int flags,
                               struct sockaddr *src_addr, socklen_t *addrlen);
EXTERN_C ssize_t __m3_recvmsg(int sockfd, struct msghdr *msg, int flags);
EXTERN_C int __m3_shutdown(int sockfd, int how);
EXTERN_C void __m3_socket_close(int fd);

// process syscalls
EXTERN_C int __m3_getpid();
EXTERN_C int __m3_getuid();
EXTERN_C int __m3_geteuid();
EXTERN_C int __m3_getgid();
EXTERN_C int __m3_getegid();
EXTERN_C mode_t __m3_umask(mode_t mode);

// time syscalls
EXTERN_C int __m3_clock_gettime(clockid_t clockid, struct timespec *tp);
EXTERN_C int __m3_nanosleep(const struct timespec *req, struct timespec *rem);

// misc
EXTERN_C int __m3_uname(struct utsname *buf);
EXTERN_C int __m3_ioctl(int fd, unsigned long request, ...);
