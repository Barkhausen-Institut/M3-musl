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

#include <base/Common.h>
#include <m3/tiles/Activity.h>
#include <m3/vfs/Dir.h>
#include <m3/vfs/File.h>
#include <m3/vfs/FileTable.h>
#include <m3/vfs/VFS.h>
#include <fs/internal.h>

#define _GNU_SOURCE
#include <sys/uio.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>

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

    try {
        return m3::VFS::open(pathname, m3_flags);
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }
}

EXTERN_C ssize_t __m3_read(int fd, void *buf, size_t count) {
    if(fd >= m3::FileTable::MAX_FDS)
        return __m3_socket_read(fd, buf, count);

    auto file = m3::Activity::self().files()->get(fd);
    if(!file)
        return -EBADF;

    try {
        ssize_t res = file->read(buf, count);
        if(res == -1)
            return -EWOULDBLOCK;
        return res;
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }
}

EXTERN_C ssize_t __m3_readv(int fildes, const struct iovec *iov, int iovcnt) {
    auto file = m3::Activity::self().files()->get(fildes);
    if(!file)
        return -EBADF;

    ssize_t total = 0;
    for(int i = 0; i < iovcnt; ++i) {
        char *base = static_cast<char*>(iov[i].iov_base);
        size_t rem = iov[i].iov_len;
        while(rem > 0) {
            try {
                ssize_t amount = file->read(base, rem);
                if(amount == -1 && total == 0)
                    return -EWOULDBLOCK;
                if(amount == 0)
                    return total;
                rem -= static_cast<size_t>(amount);
                base += amount;
                total += amount;
            }
            catch(const m3::Exception &e) {
                return -__m3_posix_errno(e.code());
            }
        }
    }
    return total;
}

EXTERN_C ssize_t __m3_writev(int fildes, const struct iovec *iov, int iovcnt) {
    auto file = m3::Activity::self().files()->get(fildes);
    if(!file)
        return -EBADF;

    ssize_t total = 0;
    for(int i = 0; i < iovcnt; ++i) {
        char *base = static_cast<char*>(iov[i].iov_base);
        size_t rem = iov[i].iov_len;
        while(rem > 0) {
            try {
                ssize_t amount = file->write(base, rem);
                if(amount == -1 && total == 0)
                    return -EWOULDBLOCK;
                if(amount == 0)
                    return total;
                rem -= static_cast<size_t>(amount);
                base += amount;
                total += amount;
            }
            catch(const m3::Exception &e) {
                return -__m3_posix_errno(e.code());
            }
        }
    }
    return total;
}

EXTERN_C ssize_t __m3_write(int fd, const void *buf, size_t count) {
    if(fd >= m3::FileTable::MAX_FDS)
        return __m3_socket_write(fd, buf, count);

    auto file = m3::Activity::self().files()->get(fd);
    if(!file)
        return -EBADF;

    try {
        ssize_t res = file->write(buf, count);
        if(res == -1)
            return -EWOULDBLOCK;
        return res;
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }
}

EXTERN_C off_t __m3_lseek(int fd, off_t offset, int whence) {
    static_assert(SEEK_SET == M3FS_SEEK_SET, "SEEK_SET mismatch");
    static_assert(SEEK_CUR == M3FS_SEEK_CUR, "SEEK_CUR mismatch");
    static_assert(SEEK_END == M3FS_SEEK_END, "SEEK_END mismatch");

    auto file = m3::Activity::self().files()->get(fd);
    if(!file)
        return -EBADF;

    try {
        return static_cast<off_t>(file->seek(static_cast<size_t>(offset), whence));
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }
}

EXTERN_C int __m3_close(int fd) {
    if(fd >= m3::FileTable::MAX_FDS)
        return __m3_socket_close(fd);

    __m3_closedir(fd);
    m3::Activity::self().files()->remove(fd);
    return 0;
}

EXTERN_C int __m3_fcntl(int, int cmd, ... /* arg */ ) {
    switch(cmd) {
        // pretend that we support file locking
        case F_SETLK: return 0;

        default: return -ENOSYS;
    }
}

EXTERN_C int __m3_faccessat(int, const char *pathname, int mode, int) {
    m3::FileInfo info;
    m3::Errors::Code res = m3::VFS::try_stat(pathname, info);
    if(res == m3::Errors::NONE) {
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
    auto file = m3::Activity::self().files()->get(fd);
    if(!file)
        return -EBADF;

    try {
        file->sync();
        return 0;
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }
}
