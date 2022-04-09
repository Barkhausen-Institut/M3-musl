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
#include <m3/session/NetworkManager.h>
#include <m3/net/TcpSocket.h>
#include <m3/net/UdpSocket.h>
#include <m3/vfs/FileTable.h>
#include <fs/internal.h>

#define _GNU_SOURCE
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>

#include "intern.h"

struct OpenSocket {
    explicit OpenSocket() : type(-1), listen_port() {
    }

    int type;
    int listen_port;
};

static OpenSocket sockets[m3::FileTable::MAX_FDS];
static m3::NetworkManager *netmng = nullptr;

EXTERN_C int __m3_posix_errno(int m3_error);

EXTERN_C int __m3_init_netmng(const char *name) {
    if(!netmng) {
        try {
            netmng = new m3::NetworkManager(name);
        }
        catch(const m3::Exception &e) {
            return -__m3_posix_errno(e.code());
        }
    }
    return 0;
}

static int init_netmng() {
    return __m3_init_netmng("net");
}

static m3::Socket *get_socket(int fd) {
    if(static_cast<size_t>(fd) >= ARRAY_SIZE(sockets))
        return nullptr;
    if(sockets[fd].type == -1)
        return nullptr;
    return static_cast<m3::Socket*>(m3::Activity::own().files()->get(fd));
}

static m3::Endpoint sockaddr_to_ep(const struct sockaddr *addr, socklen_t addrlen) {
    if(addrlen != sizeof(struct sockaddr_in))
        return m3::Endpoint::unspecified();

    auto addr_in = reinterpret_cast<const struct sockaddr_in*>(addr);
    return m3::Endpoint(m3::IpAddr(ntohl(addr_in->sin_addr.s_addr)), ntohs(addr_in->sin_port));
}

static int ep_to_sockaddr(const m3::Endpoint &ep, struct sockaddr *addr, socklen_t *addrlen) {
    if(*addrlen < sizeof(struct sockaddr_in))
        return -ENOMEM;

    auto addr_in = reinterpret_cast<struct sockaddr_in *>(addr);
    memset(addr_in, 0, sizeof(sockaddr_in));
    addr_in->sin_family = AF_INET;
    addr_in->sin_port = ep.port;
    addr_in->sin_addr.s_addr = htonl(ep.addr.addr());
    *addrlen = sizeof(struct sockaddr_in);
    return 0;
}

EXTERN_C int __m3_socket(int domain, int type, int) {
    int res;
    if((res = init_netmng()) < 0)
        return res;

    if(domain != AF_INET)
        return -ENOTSUP;

    m3::File *file = nullptr;
    try {
        switch(type & ~(SOCK_CLOEXEC | SOCK_NONBLOCK)) {
            case SOCK_STREAM:
                file = m3::TcpSocket::create(*netmng).release();
                break;
            case SOCK_DGRAM:
                file = m3::UdpSocket::create(*netmng).release();
                break;
            default:
                return -EPROTONOSUPPORT;
        }
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }

    fd_t fd = file->fd();
    assert(sockets[fd].type == -1);
    sockets[fd].type = type & ~(SOCK_CLOEXEC | SOCK_NONBLOCK);
    sockets[fd].listen_port = 0;
    return fd;
}

EXTERN_C int __m3_getsockname(int fd, struct sockaddr *addr, socklen_t *addrlen) {
    auto *s = get_socket(fd);
    if(!s)
        return -EBADF;

    m3::Endpoint ep = s->local_endpoint();
    return ep_to_sockaddr(ep, addr, addrlen);
}

EXTERN_C int __m3_getpeername(int fd, struct sockaddr *addr, socklen_t *addrlen) {
    auto *s = get_socket(fd);
    if(!s)
        return -EBADF;

    m3::Endpoint ep = s->remote_endpoint();
    return ep_to_sockaddr(ep, addr, addrlen);
}

EXTERN_C int __m3_bind(int fd, const struct sockaddr *addr, socklen_t addrlen) {
    auto *s = get_socket(fd);
    if(!s)
        return -EBADF;

    auto ep = sockaddr_to_ep(addr, addrlen);
    if(ep == m3::Endpoint::unspecified())
        return -EINVAL;

    try {
        switch(sockets[fd].type) {
            case SOCK_STREAM:
                // just remember the port for the listen call
                sockets[fd].listen_port = ep.port;
                return 0;
            case SOCK_DGRAM:
                static_cast<m3::UdpSocket*>(s)->bind(ep.port);
                return 0;
        }
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }
    UNREACHED;
}

EXTERN_C int __m3_listen(int fd, int) {
    auto *s = get_socket(fd);
    if(!s)
        return -EBADF;

    switch(sockets[fd].type) {
        case SOCK_STREAM:
            // don't do anything; accept will start listening
            return 0;
        case SOCK_DGRAM:
            return -ENOTSUP;
    }
    UNREACHED;
}

EXTERN_C int __m3_accept(int fd, struct sockaddr *addr, socklen_t *addrlen) {
    auto *s = get_socket(fd);
    if(!s)
        return -EBADF;
    if(sockets[fd].type != SOCK_STREAM)
        return -ENOTSUP;

    try {
        // create a new socket for the to-be-accepted client
        int cfd = __m3_socket(AF_INET, SOCK_STREAM, 0);
        if(cfd < 0)
            return cfd;

        // put the socket into listen mode
        auto *cs = static_cast<m3::TcpSocket*>(get_socket(cfd));
        assert(cs != nullptr);
        try {
            cs->listen(sockets[fd].listen_port);
        }
        catch(const m3::Exception &e) {
            __m3_socket_close(cfd);
            return -__m3_posix_errno(e.code());
        }

        // accept the client connection
        m3::Endpoint ep;
        cs->accept(&ep);

        // retrieve address
        if(addr) {
            int res = ep_to_sockaddr(cs->remote_endpoint(), addr, addrlen);
            if(res < 0) {
                __m3_socket_close(cfd);
                return res;
            }
        }
        return cfd;
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }
    UNREACHED;
}

EXTERN_C int __m3_accept4(int fd, struct sockaddr *addr, socklen_t *addrlen, int flags) {
    if(flags != 0)
        return -ENOTSUP;
    return __m3_accept(fd, addr, addrlen);
}

EXTERN_C int __m3_connect(int fd, const struct sockaddr *addr, socklen_t addrlen) {
    auto *s = get_socket(fd);
    if(!s)
        return -EBADF;

    auto ep = sockaddr_to_ep(addr, addrlen);
    if(ep == m3::Endpoint::unspecified())
        return -EINVAL;

    try {
        s->connect(ep);
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }
    return 0;
}

EXTERN_C ssize_t __m3_sendto(int fd, const void *buf, size_t len, int flags,
                             const struct sockaddr *dest_addr, socklen_t addrlen) {
    if(flags != 0)
        return -ENOTSUP;
    if(dest_addr == nullptr)
        return __m3_write(fd, buf, len);

    auto *s = get_socket(fd);
    if(!s)
        return -EBADF;

    try {
        switch(sockets[fd].type) {
            case SOCK_STREAM:
                return s->send(buf, len);
            case SOCK_DGRAM: {
                auto ep = sockaddr_to_ep(dest_addr, addrlen);
                if(ep == m3::Endpoint::unspecified())
                    return -EINVAL;
                return static_cast<m3::UdpSocket*>(s)->send_to(buf, len, ep);
            }
        }
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }
    UNREACHED;
}

EXTERN_C ssize_t __m3_sendmsg(int fd, const struct msghdr *msg, int flags) {
    if(msg->msg_control != nullptr || flags != 0 || msg->msg_iovlen != 1 || msg->msg_name != nullptr)
        return -ENOTSUP;

    return __m3_write(fd, msg->msg_iov->iov_base, msg->msg_iov->iov_len);
}

EXTERN_C ssize_t __m3_recvfrom(int fd, void *buf, size_t len, int flags,
                               struct sockaddr *src_addr, socklen_t *addrlen) {
    if(flags != 0)
        return -ENOTSUP;
    if(src_addr == nullptr)
        return __m3_read(fd, buf, len);

    auto *s = get_socket(fd);
    if(!s)
        return -EBADF;

    m3::Endpoint ep;
    ssize_t res = 0;
    try {
        switch(sockets[fd].type) {
            case SOCK_STREAM:
                res = s->recv(buf, len);
                ep = s->remote_endpoint();
                break;
            case SOCK_DGRAM: {
                res = static_cast<m3::UdpSocket*>(s)->recv_from(buf, len, &ep);
                break;
            }
        }
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }

    if(src_addr) {
        int conv_res = ep_to_sockaddr(ep, src_addr, addrlen);
        if(conv_res < 0)
            return conv_res;
    }
    return res;
}

EXTERN_C ssize_t __m3_recvmsg(int fd, struct msghdr *msg, int flags) {
    if(msg->msg_control != nullptr || flags != 0 || msg->msg_iovlen != 1 || msg->msg_name != nullptr)
        return -ENOTSUP;

    return __m3_read(fd, msg->msg_iov->iov_base, msg->msg_iov->iov_len);
}

EXTERN_C int __m3_shutdown(int fd, int how) {
    if(how != SHUT_RDWR)
        return -ENOTSUP;

    auto *s = get_socket(fd);
    if(!s)
        return -EBADF;

    try {
        switch(sockets[fd].type) {
            case SOCK_STREAM:
                static_cast<m3::TcpSocket*>(s)->abort();
                return 0;
            case SOCK_DGRAM:
                // nothing to do
                return 0;
        }
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }
    UNREACHED;
}

EXTERN_C void __m3_socket_close(int fd) {
    auto *s = get_socket(fd);
    if(!s)
        return;

    sockets[fd].type = -1;
    sockets[fd].listen_port = 0;
}
