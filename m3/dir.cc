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

#include "intern.h"

struct OpenDir {
    void *dir;
    m3::Dir::Entry *entry;
};

static OpenDir open_dirs[m3::FileTable::MAX_FDS];

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
    if(res != m3::Errors::NONE)
        return -__m3_posix_errno(res);
    translate_stat(info, statbuf);
    return 0;
}

EXTERN_C int __m3_fstatat(int, const char *pathname, struct kstat *statbuf, int) {
    m3::FileInfo info;
    m3::Errors::Code res = __m3c_stat(pathname, &info);
    if(res != m3::Errors::NONE)
        return -__m3_posix_errno(res);
    translate_stat(info, statbuf);
    return 0;
}

EXTERN_C ssize_t __m3_getdents64(int fd, void *dirp, size_t count) {
    if(static_cast<size_t>(fd) >= m3::FileTable::MAX_FDS)
        return -ENOSPC;

    bool read_first = true;
    if(open_dirs[fd].dir == nullptr) {
        m3::Errors::Code res = __m3c_opendir(fd, &open_dirs[fd].dir);
        if(res != m3::Errors::NONE)
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
            else if(res != m3::Errors::NONE)
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
    if(res != m3::Errors::NONE)
        return -__m3_posix_errno(res);
    return static_cast<ssize_t>(len);
}
