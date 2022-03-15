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

constexpr int FIRST_SOCK = m3::FileTable::MAX_FDS;
constexpr int MAX_SOCKETS = 32;

struct OpenSocket {
    int type;
    int listen_port;
    union {
        m3::Reference<m3::TcpSocket> stream;
        m3::Reference<m3::UdpSocket> dgram;
    };

    explicit OpenSocket(m3::Reference<m3::TcpSocket> _stream)
        : type(SOCK_STREAM),
          listen_port(),
          stream(_stream) {
    }

    explicit OpenSocket(m3::Reference<m3::UdpSocket> _dgram)
        : type(SOCK_DGRAM),
          listen_port(),
          dgram(_dgram) {
    }

    ~OpenSocket() {
        switch(type) {
            case SOCK_STREAM:
                stream.unref();
                break;
            case SOCK_DGRAM:
                dgram.unref();
                break;
        }
    }
};

static OpenSocket *sockets[MAX_SOCKETS];
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

static OpenSocket *get_socket(int fd) {
    if(fd - FIRST_SOCK >= MAX_SOCKETS)
        return nullptr;

    auto *s = sockets[fd - FIRST_SOCK];
    if(!s)
        return nullptr;
    return s;
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

    int fd = 0;
    for(; fd < MAX_SOCKETS; ++fd) {
        if(!sockets[fd])
            break;
    }
    if(fd == MAX_SOCKETS)
        return -EMFILE;

    switch(type & ~(SOCK_CLOEXEC | SOCK_NONBLOCK)) {
        case SOCK_STREAM:
            sockets[fd] = new OpenSocket(m3::TcpSocket::create(*netmng));
            break;
        case SOCK_DGRAM:
            sockets[fd] = new OpenSocket(m3::UdpSocket::create(*netmng));
            break;
        default:
            return -EPROTONOSUPPORT;
    }
    return fd + FIRST_SOCK;
}

EXTERN_C int __m3_getsockname(int fd, struct sockaddr *addr, socklen_t *addrlen) {
    auto *s = get_socket(fd);
    if(!s)
        return -EBADF;

    m3::Endpoint ep;
    try {
        switch(s->type) {
            case SOCK_STREAM:
                ep = s->stream->local_endpoint();
                break;
            case SOCK_DGRAM:
                ep = s->dgram->local_endpoint();
                break;
        }
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }

    return ep_to_sockaddr(ep, addr, addrlen);
}

EXTERN_C int __m3_getpeername(int fd, struct sockaddr *addr, socklen_t *addrlen) {
    auto *s = get_socket(fd);
    if(!s)
        return -EBADF;

    m3::Endpoint ep;
    try {
        switch(s->type) {
            case SOCK_STREAM:
                ep = s->stream->remote_endpoint();
                break;
            case SOCK_DGRAM:
                ep = s->dgram->remote_endpoint();
                break;
        }
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }

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
        switch(s->type) {
            case SOCK_STREAM:
                // just remember the port for the listen call
                s->listen_port = ep.port;
                return 0;
            case SOCK_DGRAM:
                s->dgram->bind(ep.port);
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

    try {
        switch(s->type) {
            case SOCK_STREAM:
                // don't do anything; accept will start listening
                return 0;
            case SOCK_DGRAM:
                return -ENOTSUP;
        }
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }
    UNREACHED;
}

EXTERN_C int __m3_accept(int fd, struct sockaddr *addr, socklen_t *addrlen) {
    auto *s = get_socket(fd);
    if(!s)
        return -EBADF;

    try {
        switch(s->type) {
            case SOCK_STREAM: {
                // create a new socket for the to-be-accepted client
                int cfd = __m3_socket(AF_INET, SOCK_STREAM, 0);
                if(cfd < 0)
                    return cfd;

                // put the socket into listen mode
                auto *cs = get_socket(cfd);
                assert(cs != nullptr);
                try {
                    cs->stream->listen(s->listen_port);
                }
                catch(const m3::Exception &e) {
                    __m3_socket_close(cfd);
                    return -__m3_posix_errno(e.code());
                }

                // accept the client connection
                m3::Endpoint ep;
                cs->stream->accept(&ep);

                // retrieve address
                if(addr) {
                    int res = ep_to_sockaddr(cs->stream->remote_endpoint(), addr, addrlen);
                    if(res < 0) {
                        __m3_socket_close(cfd);
                        return res;
                    }
                }
                return cfd;
            }
            case SOCK_DGRAM:
                return -ENOTSUP;
        }
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
        switch(s->type) {
            case SOCK_STREAM:
                s->stream->connect(ep);
                return 0;
            case SOCK_DGRAM:
                s->dgram->connect(ep);
                return 0;
        }
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }
    UNREACHED;
}

EXTERN_C ssize_t __m3_sendto(int fd, const void *buf, size_t len, int flags,
                             const struct sockaddr *dest_addr, socklen_t addrlen) {
    if(flags != 0)
        return -ENOTSUP;
    if(dest_addr == nullptr)
        return __m3_socket_write(fd, buf, len);

    auto *s = get_socket(fd);
    if(!s)
        return -EBADF;

    try {
        switch(s->type) {
            case SOCK_STREAM:
                return s->stream->send(buf, len);
            case SOCK_DGRAM: {
                auto ep = sockaddr_to_ep(dest_addr, addrlen);
                if(ep == m3::Endpoint::unspecified())
                    return -EINVAL;
                return s->dgram->send_to(buf, len, ep);
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

    return __m3_socket_write(fd, msg->msg_iov->iov_base, msg->msg_iov->iov_len);
}

EXTERN_C ssize_t __m3_socket_write(int fd, const void *data, size_t len) {
    auto *s = get_socket(fd);
    if(!s)
        return -EBADF;

    try {
        switch(s->type) {
            case SOCK_STREAM:
                return s->stream->send(data, len);
            case SOCK_DGRAM:
                return s->dgram->send(data, len);
        }
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }
    UNREACHED;
}

EXTERN_C ssize_t __m3_recvfrom(int fd, void *buf, size_t len, int flags,
                               struct sockaddr *src_addr, socklen_t *addrlen) {
    if(flags != 0)
        return -ENOTSUP;
    if(src_addr == nullptr)
        return __m3_socket_read(fd, buf, len);

    auto *s = get_socket(fd);
    if(!s)
        return -EBADF;

    m3::Endpoint ep;
    ssize_t res = 0;
    try {
        switch(s->type) {
            case SOCK_STREAM:
                res = s->stream->recv(buf, len);
                ep = s->stream->remote_endpoint();
                break;
            case SOCK_DGRAM: {
                res = s->dgram->recv_from(buf, len, &ep);
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

    return __m3_socket_read(fd, msg->msg_iov->iov_base, msg->msg_iov->iov_len);
}

EXTERN_C ssize_t __m3_socket_read(int fd, void *data, size_t len) {
    auto *s = get_socket(fd);
    if(!s)
        return -EBADF;

    try {
        switch(s->type) {
            case SOCK_STREAM:
                return s->stream->recv(data, len);
            case SOCK_DGRAM:
                return s->dgram->recv(data, len);
        }
    }
    catch(const m3::Exception &e) {
        return -__m3_posix_errno(e.code());
    }
    UNREACHED;
}

EXTERN_C int __m3_shutdown(int fd, int how) {
    if(how != SHUT_RDWR)
        return -ENOTSUP;

    auto *s = get_socket(fd);
    if(!s)
        return -EBADF;

    try {
        switch(s->type) {
            case SOCK_STREAM:
                s->stream->abort();
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

EXTERN_C int __m3_socket_close(int fd) {
    auto *s = get_socket(fd);
    if(!s)
        return -EBADF;

    delete s;
    sockets[fd] = nullptr;
    return 0;
}
