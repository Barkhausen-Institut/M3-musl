#include <base/Common.h>
#include <m3/pes/VPE.h>
#include <m3/vfs/File.h>
#include <m3/vfs/FileTable.h>
#include <m3/vfs/VFS.h>
#include <fs/internal.h>

#include <sys/uio.h>
#include <errno.h>
#include <fcntl.h>

static int __m3_posix_errno(int m3_error) {
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
        default: return ENOSYS;
    }
}

EXTERN_C int __m3_openat(int, const char *pathname, int flags, mode_t) {
    int m3_flags = 0;
    switch(flags & O_RDWR) {
        case O_RDONLY: m3_flags = m3::FILE_R; break;
        case O_WRONLY: m3_flags = m3::FILE_W; break;
        case O_RDWR: m3_flags = m3::FILE_RW; break;
    }

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

EXTERN_C int __m3_close(int fd) {
    m3::VPE::self().fds()->remove(fd);
    return 0;
}
