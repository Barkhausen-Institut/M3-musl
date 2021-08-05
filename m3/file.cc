#include <base/Common.h>
#include <m3/pes/VPE.h>
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

EXTERN_C int __m3_posix_errno(int m3_error);
EXTERN_C void __m3_closedir(int fd);

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
    auto file = m3::VPE::self().fds()->get(fd);
    if(!file)
        return -EBADF;

    try {
        return static_cast<ssize_t>(file->read(buf, count));
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }
}

EXTERN_C ssize_t __m3_readv(int fildes, const struct iovec *iov, int iovcnt) {
    auto file = m3::VPE::self().fds()->get(fildes);
    if(!file)
        return -EBADF;

    ssize_t total = 0;
    for(int i = 0; i < iovcnt; ++i) {
        char *base = static_cast<char*>(iov[i].iov_base);
        size_t rem = iov[i].iov_len;
        while(rem > 0) {
            try {
                size_t amount = file->read(base, rem);
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
    auto file = m3::VPE::self().fds()->get(fildes);
    if(!file)
        return -EBADF;

    ssize_t total = 0;
    for(int i = 0; i < iovcnt; ++i) {
        char *base = static_cast<char*>(iov[i].iov_base);
        size_t rem = iov[i].iov_len;
        while(rem > 0) {
            try {
                size_t amount = file->write(base, rem);
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
    auto file = m3::VPE::self().fds()->get(fd);
    if(!file)
        return -EBADF;

    try {
        return static_cast<ssize_t>(file->write(buf, count));
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }
}

EXTERN_C off_t __m3_lseek(int fd, off_t offset, int whence) {
    static_assert(SEEK_SET == M3FS_SEEK_SET, "SEEK_SET mismatch");
    static_assert(SEEK_CUR == M3FS_SEEK_CUR, "SEEK_CUR mismatch");
    static_assert(SEEK_END == M3FS_SEEK_END, "SEEK_END mismatch");

    auto file = m3::VPE::self().fds()->get(fd);
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
    __m3_closedir(fd);
    m3::VPE::self().fds()->remove(fd);
    return 0;
}

EXTERN_C int __m3_fcntl(int, int cmd, ... /* arg */ ) {
    switch(cmd) {
        // pretend that we support file locking
        case F_SETLK: return 0;

        default: return -ENOSYS;
    }
}

EXTERN_C int __m3_faccessat(int dirfd, const char *pathname, int mode, int) {
    int oflags;
    if(mode == R_OK || mode == F_OK)
        oflags = O_RDONLY;
    else if(mode == W_OK)
        oflags = O_WRONLY;
    else
        oflags = O_RDWR;

    // TODO provide a more efficient implementation
    int fd = __m3_openat(dirfd, pathname, oflags, 0);
    if(fd >= 0)
        __m3_close(fd);
    return fd < 0 ? fd : 0;
}

EXTERN_C int __m3_fsync(int fd) {
    auto file = m3::VPE::self().fds()->get(fd);
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
