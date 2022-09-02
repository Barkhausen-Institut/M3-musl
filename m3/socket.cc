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

#include <errno.h>
#include <fcntl.h>
#include <fs/internal.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "intern.h"

struct OpenSocket {
    explicit OpenSocket() : type(INVALID), listen_port() {
    }

    CompatSock type;
    int listen_port;
};

static OpenSocket sockets[m3::FileTable::MAX_FDS];

EXTERN_C int __m3_posix_errno(int m3_error);

static bool check_socket(int fd) {
    if(static_cast<size_t>(fd) >= ARRAY_SIZE(sockets))
        return false;
    if(sockets[fd].type == INVALID)
        return false;
    return true;
}

static CompatEndpoint sockaddr_to_ep(const struct sockaddr *addr, socklen_t addrlen) {
    CompatEndpoint ep = {
        .addr = 0,
        .port = 0,
    };
    if(addrlen != sizeof(struct sockaddr_in))
        return ep;

    auto addr_in = reinterpret_cast<const struct sockaddr_in *>(addr);
    ep.addr = ntohl(addr_in->sin_addr.s_addr);
    ep.port = ntohs(addr_in->sin_port);
    return ep;
}

static int ep_to_sockaddr(CompatEndpoint &ep, struct sockaddr *addr, socklen_t *addrlen) {
    if(*addrlen < sizeof(struct sockaddr_in))
        return -ENOMEM;

    auto addr_in = reinterpret_cast<struct sockaddr_in *>(addr);
    memset(addr_in, 0, sizeof(sockaddr_in));
    addr_in->sin_family = AF_INET;
    addr_in->sin_port = htons(ep.port);
    addr_in->sin_addr.s_addr = htonl(ep.addr);
    *addrlen = sizeof(struct sockaddr_in);
    return 0;
}

EXTERN_C int __m3_socket(int domain, int type, int) {
    if(domain != AF_INET)
        return -ENOTSUP;

    CompatSock stype;
    switch(type & ~(SOCK_CLOEXEC | SOCK_NONBLOCK)) {
        case SOCK_STREAM: stype = CompatSock::STREAM; break;
        case SOCK_DGRAM: stype = CompatSock::DGRAM; break;
        default: return -EPROTONOSUPPORT;
    }

    int fd;
    m3::Errors::Code res = __m3c_socket(stype, &fd);
    if(res != m3::Errors::NONE)
        return -__m3_posix_errno(res);

    assert(sockets[fd].type == INVALID);
    sockets[fd].type = stype;
    sockets[fd].listen_port = 0;
    return fd;
}

EXTERN_C int __m3_getsockname(int fd, struct sockaddr *addr, socklen_t *addrlen) {
    if(!check_socket(fd))
        return -EBADF;
    CompatEndpoint ep;
    m3::Errors::Code res = __m3c_get_local_ep(fd, sockets[fd].type, &ep);
    if(res != m3::Errors::NONE)
        return -__m3_posix_errno(res);
    return ep_to_sockaddr(ep, addr, addrlen);
}

EXTERN_C int __m3_getpeername(int fd, struct sockaddr *addr, socklen_t *addrlen) {
    if(!check_socket(fd))
        return -EBADF;
    CompatEndpoint ep;
    m3::Errors::Code res = __m3c_get_remote_ep(fd, sockets[fd].type, &ep);
    if(res != m3::Errors::NONE)
        return -__m3_posix_errno(res);
    return ep_to_sockaddr(ep, addr, addrlen);
}

EXTERN_C int __m3_bind(int fd, const struct sockaddr *addr, socklen_t addrlen) {
    if(!check_socket(fd))
        return -EBADF;

    auto ep = sockaddr_to_ep(addr, addrlen);
    if(ep.addr == 0 && ep.port == 0)
        return -EINVAL;

    switch(sockets[fd].type) {
        case CompatSock::STREAM:
            // just remember the port for the listen call
            sockets[fd].listen_port = ep.port;
            return 0;
        default:
        case CompatSock::DGRAM: return -__m3_posix_errno(__m3c_bind_dgram(fd, &ep));
    }
    UNREACHED;
}

EXTERN_C int __m3_listen(int fd, int) {
    if(!check_socket(fd))
        return -EBADF;

    switch(sockets[fd].type) {
        case CompatSock::STREAM:
            // don't do anything; accept will start listening
            return 0;
        default:
        case CompatSock::DGRAM: return -ENOTSUP;
    }
    UNREACHED;
}

EXTERN_C int __m3_accept(int fd, struct sockaddr *addr, socklen_t *addrlen) {
    if(!check_socket(fd))
        return -EBADF;
    if(sockets[fd].type != CompatSock::STREAM)
        return -ENOTSUP;

    int cfd;
    CompatEndpoint ep;
    m3::Errors::Code res = __m3c_accept_stream(sockets[fd].listen_port, &cfd, &ep);
    if(res != m3::Errors::NONE)
        return res;

    assert(sockets[cfd].type == INVALID);
    sockets[cfd].type = CompatSock::STREAM;
    sockets[cfd].listen_port = 0;

    // retrieve address
    if(addr) {
        int res = ep_to_sockaddr(ep, addr, addrlen);
        if(res < 0) {
            __m3_socket_close(cfd);
            return res;
        }
    }
    return cfd;
}

EXTERN_C int __m3_accept4(int fd, struct sockaddr *addr, socklen_t *addrlen, int flags) {
    if(flags != 0)
        return -ENOTSUP;
    return __m3_accept(fd, addr, addrlen);
}

EXTERN_C int __m3_connect(int fd, const struct sockaddr *addr, socklen_t addrlen) {
    if(!check_socket(fd))
        return -EBADF;

    auto ep = sockaddr_to_ep(addr, addrlen);
    if(ep.addr == 0 && ep.port == 0)
        return -EINVAL;

    return -__m3_posix_errno(__m3c_connect(fd, sockets[fd].type, &ep));
}

EXTERN_C ssize_t __m3_sendto(int fd, const void *buf, size_t len, int flags,
                             const struct sockaddr *dest_addr, socklen_t addrlen) {
    // we don't have signals anyway, so we can allow this flag
    if((flags & ~MSG_NOSIGNAL) != 0)
        return -ENOTSUP;
    if(dest_addr == nullptr)
        return __m3_write(fd, buf, len);
    if(!check_socket(fd))
        return -EBADF;

    CompatEndpoint ep;
    if(sockets[fd].type == CompatSock::DGRAM) {
        ep = sockaddr_to_ep(dest_addr, addrlen);
        if(ep.addr == 0 && ep.port == 0)
            return -EINVAL;
    }

    m3::Errors::Code res = __m3c_sendto(fd, sockets[fd].type, buf, &len, &ep);
    if(res != m3::Errors::NONE)
        return -__m3_posix_errno(res);
    return static_cast<ssize_t>(len);
}

EXTERN_C ssize_t __m3_sendmsg(int fd, const struct msghdr *msg, int flags) {
    if(msg->msg_control != nullptr || flags != 0 || msg->msg_iovlen != 1 ||
       msg->msg_name != nullptr)
        return -ENOTSUP;

    return __m3_write(fd, msg->msg_iov->iov_base, msg->msg_iov->iov_len);
}

EXTERN_C ssize_t __m3_recvfrom(int fd, void *buf, size_t len, int flags, struct sockaddr *src_addr,
                               socklen_t *addrlen) {
    // we don't have signals anyway, so we can allow this flag
    if((flags & ~MSG_NOSIGNAL) != 0)
        return -ENOTSUP;
    if(src_addr == nullptr)
        return __m3_read(fd, buf, len);
    if(!check_socket(fd))
        return -EBADF;

    CompatEndpoint ep;
    m3::Errors::Code res = __m3c_recvfrom(fd, sockets[fd].type, buf, &len, &ep);
    if(res != m3::Errors::NONE)
        return -__m3_posix_errno(res);
    if(src_addr) {
        int conv_res = ep_to_sockaddr(ep, src_addr, addrlen);
        if(conv_res < 0)
            return conv_res;
    }
    return static_cast<ssize_t>(len);
}

EXTERN_C ssize_t __m3_recvmsg(int fd, struct msghdr *msg, int flags) {
    if(msg->msg_control != nullptr || flags != 0 || msg->msg_iovlen != 1 ||
       msg->msg_name != nullptr)
        return -ENOTSUP;

    return __m3_read(fd, msg->msg_iov->iov_base, msg->msg_iov->iov_len);
}

EXTERN_C int __m3_shutdown(int fd, int how) {
    if(how != SHUT_RDWR)
        return -ENOTSUP;
    if(!check_socket(fd))
        return -EBADF;

    switch(sockets[fd].type) {
        case CompatSock::STREAM: return -__m3_posix_errno(__m3c_abort_stream(fd));
        default:
        case CompatSock::DGRAM:
            // nothing to do
            return 0;
    }
    UNREACHED;
}

EXTERN_C void __m3_socket_close(int fd) {
    if(!check_socket(fd))
        return;

    sockets[fd].type = INVALID;
    sockets[fd].listen_port = 0;
}
