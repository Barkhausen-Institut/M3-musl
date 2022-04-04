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

// #define _GNU_SOURCE
#include <sys/stat.h>
#include <sys/uio.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <kstat.h>

#include "intern.h"

struct OpenDir {
    m3::Dir *dir;
    m3::Dir::Entry *entry;
};

static const size_t MAX_DIRS = 16;
static OpenDir open_dirs[MAX_DIRS];

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
    auto file = m3::Activity::own().files()->get(fd);
    if(!file)
        return -EBADF;

    m3::FileInfo info;
    m3::Errors::Code res = file->try_stat(info);
    if(res != m3::Errors::NONE)
        return -__m3_posix_errno(res);
    translate_stat(info, statbuf);
    return 0;
}

EXTERN_C int __m3_fstatat(int, const char *pathname,
                          struct kstat *statbuf, int) {
    m3::FileInfo info;
    m3::Errors::Code res = m3::VFS::try_stat(pathname, info);
    if(res != m3::Errors::NONE)
        return -__m3_posix_errno(res);
    translate_stat(info, statbuf);
    return 0;
}

EXTERN_C ssize_t __m3_getdents64(int fd, void *dirp, size_t count) {
    auto file = m3::Activity::own().files()->get(fd);
    if(!file)
        return -EBADF;
    if(static_cast<size_t>(fd) >= MAX_DIRS)
        return -ENOSPC;

    bool read_first = true;
    if(open_dirs[fd].dir == nullptr) {
        try {
            open_dirs[fd].dir = new m3::Dir(fd);
            open_dirs[fd].entry = new m3::Dir::Entry;
        }
        catch(const m3::Exception &e) {
            return -__m3_posix_errno(e.code());
        }
    }
    // have we already seen EOF?
    else if(open_dirs[fd].entry == nullptr)
        return 0;
    // if this is not the first call and we didn't see EOF, the entry has always already been read
    else
        read_first = false;

    char *cur_dirp = reinterpret_cast<char*>(dirp);
    size_t rem = count;
    while(true) {
        try {
            if(read_first && !open_dirs[fd].dir->readdir(*open_dirs[fd].entry)) {
                // EOF; remember that we've seen it
                delete open_dirs[fd].entry;
                open_dirs[fd].entry = nullptr;
                break;
            }
        }
        catch(const m3::Exception &e) {
            return -__m3_posix_errno(e.code());
        }

        auto *de = reinterpret_cast<struct dirent*>(cur_dirp);
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
    return -__m3_posix_errno(m3::VFS::try_mkdir(pathname, mode));
}

EXTERN_C int __m3_renameat2(int, const char *oldpath,
                            int, const char *newpath, unsigned int) {
    return -__m3_posix_errno(m3::VFS::try_rename(oldpath, newpath));
}

EXTERN_C int __m3_unlinkat(int, const char *pathname, int flags) {
    if(flags & AT_REMOVEDIR)
        return -__m3_posix_errno(m3::VFS::try_rmdir(pathname));
    else
        return -__m3_posix_errno(m3::VFS::try_unlink(pathname));
}

EXTERN_C void __m3_closedir(int fd) {
    if(static_cast<size_t>(fd) < MAX_DIRS) {
        delete open_dirs[fd].dir;
        delete open_dirs[fd].entry;
        open_dirs[fd].dir = nullptr;
        open_dirs[fd].entry = nullptr;
    }
}
