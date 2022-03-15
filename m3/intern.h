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

#include <sys/socket.h>

EXTERN_C int __m3_posix_errno(int m3_error);

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
EXTERN_C void __m3_closedir(int fd);
