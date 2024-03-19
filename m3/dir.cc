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

#include <m3/Compat.h>

#ifdef __cplusplus
#    define restrict __restrict
#endif

#include <bits/syscall.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fs/internal.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/uio.h>
// clang-format off
#include <kstat.h> // needs to be last so that we have dev_t etc.
// clang-format on

#undef restrict

#include "intern.h"

struct OpenDir {
    void *dir;
    m3::Dir::Entry *entry;
};

static OpenDir open_dirs[m3::FileTable::MAX_FDS];

struct statx {
    uint32_t stx_mask;
    uint32_t stx_blksize;
    uint64_t stx_attributes;
    uint32_t stx_nlink;
    uint32_t stx_uid;
    uint32_t stx_gid;
    uint16_t stx_mode;
    uint16_t pad1;
    uint64_t stx_ino;
    uint64_t stx_size;
    uint64_t stx_blocks;
    uint64_t stx_attributes_mask;
    struct {
        int64_t tv_sec;
        uint32_t tv_nsec;
        int32_t pad;
    } stx_atime, stx_btime, stx_ctime, stx_mtime;
    uint32_t stx_rdev_major;
    uint32_t stx_rdev_minor;
    uint32_t stx_dev_major;
    uint32_t stx_dev_minor;
    uint64_t spare[14];
};

static void translate_stat(m3::FileInfo &info, struct statx *statbuf) {
    statbuf->stx_mask = 0xfffU /* STATX_ALL */;
    statbuf->stx_blksize = 4096;
    statbuf->stx_attributes = 0;
    statbuf->stx_nlink = info.links;
    statbuf->stx_uid = 0;
    statbuf->stx_gid = 0;
    statbuf->stx_mode = info.mode;
    statbuf->stx_ino = info.inode;
    statbuf->stx_size = static_cast<uint64_t>(info.size);
    statbuf->stx_blocks = static_cast<uint64_t>((info.size + 4096 - 1) / 4096);
    statbuf->stx_attributes_mask = 0;
    statbuf->stx_dev_major = info.devno;
    statbuf->stx_dev_minor = 0;
    statbuf->stx_rdev_major = info.devno;
    statbuf->stx_rdev_minor = 0;
    statbuf->stx_atime.tv_sec = static_cast<long>(info.lastaccess);
    statbuf->stx_atime.tv_nsec = 0;
    statbuf->stx_mtime.tv_sec = static_cast<long>(info.lastmod);
    statbuf->stx_mtime.tv_nsec = 0;
    statbuf->stx_ctime.tv_sec = static_cast<long>(info.lastmod);
    statbuf->stx_ctime.tv_nsec = 0;
}

EXTERN_C int __m3_statx(int fd, const char *pathname, int flags, unsigned int,
                        struct statx *statbuf) {
    m3::FileInfo info;
    m3::Errors::Code res;
    if(flags & 0x1000 /* AT_EMPTY_PATH */)
        res = __m3c_fstat(fd, &info);
    else
        res = __m3c_stat(pathname, &info);
    if(res != m3::Errors::SUCCESS)
        return -__m3_posix_errno(res);
    translate_stat(info, statbuf);
    return 0;
}

#if !defined(__riscv) || __riscv_xlen != 32
static void translate_stat(m3::FileInfo &info, struct kstat *statbuf) {
    statbuf->st_dev = info.devno;
    statbuf->st_ino = info.inode;
    statbuf->st_mode = info.mode;
    statbuf->st_nlink = info.links;
    statbuf->st_uid = 0;
    statbuf->st_gid = 0;
    statbuf->st_rdev = info.devno;
    statbuf->st_size = static_cast<off_t>(info.size);
    statbuf->st_blksize = 4096;
    statbuf->st_blocks = static_cast<blkcnt_t>((info.size + 4096 - 1) / 4096);
    statbuf->st_atime_sec = static_cast<long>(info.lastaccess);
    statbuf->st_atime_nsec = 0;
    statbuf->st_mtime_sec = static_cast<long>(info.lastmod);
    statbuf->st_mtime_nsec = 0;
    statbuf->st_ctime_sec = static_cast<long>(info.lastmod);
    statbuf->st_ctime_nsec = 0;
}

EXTERN_C int __m3_fstat(int fd, struct kstat *statbuf) {
    m3::FileInfo info;
    m3::Errors::Code res = __m3c_fstat(fd, &info);
    if(res != m3::Errors::SUCCESS)
        return -__m3_posix_errno(res);
    translate_stat(info, statbuf);
    return 0;
}

EXTERN_C int __m3_fstatat(int, const char *pathname, struct kstat *statbuf, int) {
    m3::FileInfo info;
    m3::Errors::Code res = __m3c_stat(pathname, &info);
    if(res != m3::Errors::SUCCESS)
        return -__m3_posix_errno(res);
    translate_stat(info, statbuf);
    return 0;
}
#endif

EXTERN_C ssize_t __m3_getdents64(int fd, void *dirp, size_t count) {
    if(static_cast<size_t>(fd) >= m3::FileTable::MAX_FDS)
        return -ENOSPC;

    bool read_first = true;
    if(open_dirs[fd].dir == nullptr) {
        m3::Errors::Code res = __m3c_opendir(fd, &open_dirs[fd].dir);
        if(res != m3::Errors::SUCCESS)
            return -__m3_posix_errno(res);
        open_dirs[fd].entry = static_cast<m3::Dir::Entry *>(malloc(sizeof(m3::Dir::Entry)));
    }
    // have we already seen EOF?
    else if(open_dirs[fd].entry == nullptr)
        return 0;
    // if this is not the first call and we didn't see EOF, the entry has always already been read
    else
        read_first = false;

    char *cur_dirp = reinterpret_cast<char *>(dirp);
    size_t rem = count;
    while(true) {
        if(read_first) {
            m3::Errors::Code res = __m3c_readdir(open_dirs[fd].dir, open_dirs[fd].entry);
            if(res == m3::Errors::END_OF_FILE) {
                // EOF; remember that we've seen it
                free(open_dirs[fd].entry);
                open_dirs[fd].entry = nullptr;
                break;
            }
            else if(res != m3::Errors::SUCCESS)
                return -__m3_posix_errno(res);
        }

        auto *de = reinterpret_cast<struct dirent *>(cur_dirp);
        size_t namelen = strlen(open_dirs[fd].entry->name);
        size_t reclen = namelen + 1 + (sizeof(struct dirent) - sizeof(open_dirs[fd].entry->name));
        reclen = m3::Math::round_up(reclen, alignof(struct dirent));
        if(reclen > rem)
            break;

        de->d_ino = open_dirs[fd].entry->nodeno;
        de->d_off = 0;
        de->d_reclen = reclen;
        de->d_type = 0;
        memcpy(de->d_name, open_dirs[fd].entry->name, namelen + 1);
        cur_dirp += reclen;
        rem -= reclen;
        read_first = true;
    }
    return static_cast<ssize_t>(count - rem);
}

EXTERN_C int __m3_mkdirat(int, const char *pathname, mode_t mode) {
    return -__m3_posix_errno(__m3c_mkdir(pathname, mode));
}

EXTERN_C int __m3_renameat2(int, const char *oldpath, int, const char *newpath, unsigned int) {
    return -__m3_posix_errno(__m3c_rename(oldpath, newpath));
}

EXTERN_C int __m3_linkat(int, const char *oldpath, int, const char *newpath, int) {
    return -__m3_posix_errno(__m3c_link(oldpath, newpath));
}

EXTERN_C int __m3_unlinkat(int, const char *pathname, int flags) {
    if(flags & AT_REMOVEDIR)
        return -__m3_posix_errno(__m3c_rmdir(pathname));
    else
        return -__m3_posix_errno(__m3c_unlink(pathname));
}

EXTERN_C void __m3_closedir(int fd) {
    if(static_cast<size_t>(fd) < m3::FileTable::MAX_FDS && open_dirs[fd].dir != nullptr) {
        __m3c_closedir(open_dirs[fd].dir);
        free(open_dirs[fd].entry);
        open_dirs[fd].dir = nullptr;
        open_dirs[fd].entry = nullptr;
    }
}

EXTERN_C int __m3_chdir(const char *path) {
    return -__m3_posix_errno(__m3c_chdir(path));
}

EXTERN_C int __m3_fchdir(int fd) {
    return -__m3_posix_errno(__m3c_fchdir(fd));
}

EXTERN_C ssize_t __m3_getcwd(char *buf, size_t size) {
    size_t len = size;
    m3::Errors::Code res = __m3c_getcwd(buf, &len);
    if(res != m3::Errors::SUCCESS)
        return -__m3_posix_errno(res);
    return static_cast<ssize_t>(len);
}
