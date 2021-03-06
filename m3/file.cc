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

#include <base/Common.h>

#include <m3/tiles/Activity.h>
#include <m3/vfs/Dir.h>
#include <m3/vfs/File.h>
#include <m3/vfs/FileTable.h>
#include <m3/vfs/VFS.h>

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

    try {
        return m3::VFS::open(pathname, m3_flags).release()->fd();
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }
}

EXTERN_C ssize_t __m3_read(int fd, void *buf, size_t count) {
    try {
        auto file = m3::Activity::own().files()->get(fd);
        if(auto res = file->read(buf, count))
            return static_cast<ssize_t>(res.unwrap());
        return -EWOULDBLOCK;
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }
}

EXTERN_C ssize_t __m3_readv(int fildes, const struct iovec *iov, int iovcnt) {
    ssize_t total = 0;
    for(int i = 0; i < iovcnt; ++i) {
        char *base = static_cast<char *>(iov[i].iov_base);
        size_t rem = iov[i].iov_len;
        while(rem > 0) {
            try {
                auto file = m3::Activity::own().files()->get(fildes);
                auto res = file->read(base, rem);
                if(res.is_none())
                    return total == 0 ? -EWOULDBLOCK : total;

                size_t amount = res.unwrap();
                if(amount == 0)
                    return total;
                rem -= amount;
                base += amount;
                total += static_cast<ssize_t>(amount);
            }
            catch(const m3::Exception &e) {
                return -__m3_posix_errno(e.code());
            }
        }
    }
    return total;
}

EXTERN_C ssize_t __m3_writev(int fildes, const struct iovec *iov, int iovcnt) {
    m3::File *file;
    try {
        file = m3::Activity::own().files()->get(fildes);
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }

    ssize_t total = 0;
    for(int i = 0; i < iovcnt; ++i) {
        char *base = static_cast<char *>(iov[i].iov_base);
        size_t rem = iov[i].iov_len;
        while(rem > 0) {
            try {
                auto res = file->write(base, rem);
                if(res.is_none())
                    return total == 0 ? -EWOULDBLOCK : total;

                size_t amount = res.unwrap();
                if(amount == 0)
                    return total;
                rem -= amount;
                base += amount;
                total += static_cast<ssize_t>(amount);
            }
            catch(const m3::Exception &e) {
                return -__m3_posix_errno(e.code());
            }
        }
    }
    return total;
}

EXTERN_C ssize_t __m3_write(int fd, const void *buf, size_t count) {
    try {
        auto file = m3::Activity::own().files()->get(fd);
        // use write_all here, because some tools seem to expect that write can't write less than
        // requested and we don't really lose anything by calling write_all instead of write.
        if(auto res = file->write_all(buf, count))
            return static_cast<ssize_t>(res.unwrap());
        return -EWOULDBLOCK;
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }
}

EXTERN_C int __m3_fflush(int fd) {
    try {
        m3::File *file = m3::Activity::own().files()->get(fd);
        file->flush();
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }
    return 0;
}

EXTERN_C off_t __m3_lseek(int fd, off_t offset, int whence) {
    static_assert(SEEK_SET == M3FS_SEEK_SET, "SEEK_SET mismatch");
    static_assert(SEEK_CUR == M3FS_SEEK_CUR, "SEEK_CUR mismatch");
    static_assert(SEEK_END == M3FS_SEEK_END, "SEEK_END mismatch");

    try {
        auto file = m3::Activity::own().files()->get(fd);
        return static_cast<off_t>(file->seek(static_cast<size_t>(offset), whence));
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }
}

EXTERN_C int __m3_ftruncate(int fd, off_t length) {
    try {
        auto file = m3::Activity::own().files()->get(fd);
        file->truncate(static_cast<size_t>(length));
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }
    return 0;
}

EXTERN_C int __m3_truncate(const char *pathname, off_t length) {
    try {
        auto file = m3::VFS::open(pathname, m3::FILE_W);
        file->truncate(static_cast<size_t>(length));
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }
    return 0;
}

EXTERN_C int __m3_close(int fd) {
    __m3_epoll_close(fd);
    __m3_socket_close(fd);
    __m3_closedir(fd);
    m3::Activity::own().files()->remove(fd);
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
    try {
        auto file = m3::Activity::own().files()->get(fd);
        file->sync();
        return 0;
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }
}
