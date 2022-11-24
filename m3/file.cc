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

#define _GNU_SOURCE

#include <m3/Compat.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fs/internal.h>
#include <sys/uio.h>

#include "intern.h"

EXTERN_C int __m3_openat(int, const char *pathname, int flags, mode_t) {
    int m3_flags;
    if(flags & O_WRONLY)
        m3_flags = m3::FILE_W;
    else if(flags & O_RDWR)
        m3_flags = m3::FILE_RW;
    else
        m3_flags = m3::FILE_R;
    if(flags & O_CREAT)
        m3_flags |= m3::FILE_CREATE;
    if(flags & O_TRUNC)
        m3_flags |= m3::FILE_TRUNC;
    if(flags & O_APPEND)
        m3_flags |= m3::FILE_APPEND;

    int fd;
    m3::Errors::Code res = __m3c_open(pathname, m3_flags, &fd);
    if(res != m3::Errors::SUCCESS)
        return -__m3_posix_errno(res);
    return fd;
}

EXTERN_C ssize_t __m3_read(int fd, void *buf, size_t count) {
    size_t read = count;
    m3::Errors::Code res = __m3c_read(fd, buf, &read);
    if(res != m3::Errors::SUCCESS)
        return -__m3_posix_errno(res);
    return static_cast<ssize_t>(read);
}

EXTERN_C ssize_t __m3_readv(int fildes, const struct iovec *iov, int iovcnt) {
    ssize_t total = 0;
    for(int i = 0; i < iovcnt; ++i) {
        char *base = static_cast<char *>(iov[i].iov_base);
        size_t rem = iov[i].iov_len;
        while(rem > 0) {
            ssize_t res = __m3_read(fildes, base, rem);
            if(res == -EWOULDBLOCK)
                return total == 0 ? -EWOULDBLOCK : total;
            else if(res < 0)
                return res;
            else if(res == 0)
                return total;

            rem -= static_cast<size_t>(res);
            base += res;
            total += res;
        }
    }
    return total;
}

EXTERN_C ssize_t __m3_write(int fd, const void *buf, size_t count) {
    size_t written = count;
    m3::Errors::Code res = __m3c_write(fd, buf, &written);
    if(res != m3::Errors::SUCCESS)
        return -__m3_posix_errno(res);
    return static_cast<ssize_t>(written);
}

EXTERN_C ssize_t __m3_writev(int fildes, const struct iovec *iov, int iovcnt) {
    ssize_t total = 0;
    for(int i = 0; i < iovcnt; ++i) {
        char *base = static_cast<char *>(iov[i].iov_base);
        size_t rem = iov[i].iov_len;
        while(rem > 0) {
            ssize_t res = __m3_write(fildes, base, rem);
            if(res == -EWOULDBLOCK)
                return total == 0 ? -EWOULDBLOCK : total;
            else if(res < 0)
                return res;
            else if(res == 0)
                return total;

            rem -= static_cast<size_t>(res);
            base += res;
            total += res;
        }
    }
    return total;
}

EXTERN_C int __m3_fflush(int fd) {
    return -__m3_posix_errno(__m3c_fflush(fd));
}

EXTERN_C off_t __m3_lseek(int fd, off_t offset, int whence) {
    static_assert(SEEK_SET == M3FS_SEEK_SET, "SEEK_SET mismatch");
    static_assert(SEEK_CUR == M3FS_SEEK_CUR, "SEEK_CUR mismatch");
    static_assert(SEEK_END == M3FS_SEEK_END, "SEEK_END mismatch");

    size_t soffset = static_cast<size_t>(offset);
    m3::Errors::Code res = __m3c_lseek(fd, &soffset, whence);
    offset = static_cast<off_t>(soffset);
    if(res != m3::Errors::SUCCESS)
        return -__m3_posix_errno(res);
    return offset;
}

EXTERN_C int __m3_ftruncate(int fd, off_t length) {
    return -__m3_posix_errno(__m3c_ftruncate(fd, static_cast<size_t>(length)));
}

EXTERN_C int __m3_truncate(const char *pathname, off_t length) {
    return -__m3_posix_errno(__m3c_truncate(pathname, static_cast<size_t>(length)));
}

EXTERN_C int __m3_close(int fd) {
    __m3_epoll_close(fd);
    __m3_socket_close(fd);
    __m3_closedir(fd);
    __m3c_close(fd);
    return 0;
}

EXTERN_C int __m3_fcntl(int, int cmd, ... /* arg */) {
    switch(cmd) {
        // pretend that we support file locking
        case F_SETLK: return 0;

        default: return -ENOSYS;
    }
}

EXTERN_C int __m3_faccessat(int, const char *pathname, int mode, int) {
    m3::FileInfo info;
    m3::Errors::Code res = __m3c_stat(pathname, &info);
    if(res == m3::Errors::SUCCESS) {
        if(mode == R_OK || mode == F_OK)
            return (info.mode & M3FS_MODE_READ) != 0 ? 0 : EPERM;
        if(mode == W_OK)
            return (info.mode & M3FS_MODE_WRITE) != 0 ? 0 : EPERM;
        return (info.mode & M3FS_MODE_READ) != 0 && (info.mode & M3FS_MODE_WRITE) != 0 ? 0 : EPERM;
    }
    else
        return -__m3_posix_errno(res);
}

EXTERN_C int __m3_fsync(int fd) {
    return -__m3_posix_errno(__m3c_sync(fd));
}
