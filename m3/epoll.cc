/*
 * Copyright (C) 2022 Nils Asmussen, Barkhausen Institut
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
#include <sys/epoll.h>

#include "intern.h"

constexpr size_t MAX_EPOLL_DESCS = 4;
constexpr size_t MAX_EPOLL_FILES = 8;

struct EPollDesc {
    void *waiter;
    struct {
        fd_t fd;
        epoll_data data;
    } data[MAX_EPOLL_FILES];
};

static EPollDesc *descs[MAX_EPOLL_DESCS];

static EPollDesc *get_desc(int epfd) {
    if(epfd < m3::FileTable::MAX_FDS)
        return nullptr;
    epfd -= m3::FileTable::MAX_FDS;
    if(static_cast<size_t>(epfd) > MAX_EPOLL_DESCS)
        return nullptr;
    return descs[epfd];
}

EXTERN_C int __m3_epoll_create(int) {
    for(size_t i = 0; i < MAX_EPOLL_DESCS; ++i) {
        if(descs[i] == nullptr) {
            descs[i] = static_cast<EPollDesc *>(malloc(sizeof(EPollDesc)));
            descs[i]->waiter = nullptr;
            for(size_t j = 0; j < MAX_EPOLL_FILES; ++j)
                descs[i]->data[j].fd = -1;

            m3::Errors::Code res = __m3c_waiter_create(&descs[i]->waiter);
            if(res != m3::Errors::NONE) {
                free(descs[i]);
                descs[i] = nullptr;
                return -__m3_posix_errno(res);
            }
            return static_cast<int>(m3::FileTable::MAX_FDS + i);
        }
    }

    return -ENOMEM;
}

EXTERN_C int __m3_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
    EPollDesc *desc = get_desc(epfd);
    if(!desc)
        return -EBADF;

    uint events = 0;
    // socket-close events (EPOLLRDHUP | EPOLLHUP) are input events
    if(event->events & (EPOLLIN | EPOLLRDHUP | EPOLLHUP))
        events |= m3::File::INPUT;
    if(event->events & EPOLLOUT)
        events |= m3::File::OUTPUT;

    if(op != EPOLL_CTL_DEL) {
        size_t i = 0;
        for(; i < MAX_EPOLL_FILES; ++i) {
            if((op == EPOLL_CTL_MOD && desc->data[i].fd == fd) ||
               (op == EPOLL_CTL_ADD && desc->data[i].fd == -1)) {
                desc->data[i].fd = fd;
                memcpy(&desc->data[i].data, &event->data, sizeof(epoll_data));
                break;
            }
        }
        if(i == MAX_EPOLL_FILES)
            return -ENOMEM;
    }

    switch(op) {
        case EPOLL_CTL_ADD: __m3c_waiter_add(desc->waiter, fd, events); break;
        case EPOLL_CTL_MOD: __m3c_waiter_set(desc->waiter, fd, events); break;
        case EPOLL_CTL_DEL: __m3c_waiter_rem(desc->waiter, fd); break;
    }

    return 0;
}

struct pwait {
    int idx;
    int maxevents;
    EPollDesc *desc;
    struct epoll_event *events;
};

static void pwait_fetcher(void *p, int fd, uint fdevs) {
    pwait *pwait = static_cast<struct pwait *>(p);
    if(pwait->idx < pwait->maxevents) {
        pwait->events[pwait->idx].events = 0;
        if(fdevs & m3::File::INPUT)
            pwait->events[pwait->idx].events |= EPOLLIN;
        if(fdevs & m3::File::OUTPUT)
            pwait->events[pwait->idx].events |= EPOLLOUT;
        for(size_t i = 0; i < MAX_EPOLL_FILES; ++i) {
            if(pwait->desc->data[i].fd == fd) {
                memcpy(&pwait->events[pwait->idx].data, &pwait->desc->data[i].data,
                       sizeof(epoll_data));
                break;
            }
        }
        pwait->idx++;
    }
}

EXTERN_C int __m3_epoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout,
                              const sigset_t *) {
    EPollDesc *desc = get_desc(epfd);
    if(!desc)
        return -EBADF;

    if(timeout == -1)
        __m3c_waiter_wait(desc->waiter);
    else {
        // for 0 (= return immediately), we use a very short timeout to check once and count the
        // ready file descriptors
        uint64_t m3_timeout = timeout == 0 ? 1 : static_cast<uint64_t>(timeout) * 1'000'000;
        __m3c_waiter_waitfor(desc->waiter, m3_timeout);
    }

    pwait arg = {
        .idx = 0,
        .maxevents = maxevents,
        .desc = desc,
        .events = events,
    };
    __m3c_waiter_fetch(desc->waiter, &arg, &pwait_fetcher);
    return arg.idx;
}

EXTERN_C int __m3_epoll_close(int epfd) {
    EPollDesc *desc = get_desc(epfd);
    if(!desc)
        return -EBADF;

    int idx = epfd - m3::FileTable::MAX_FDS;
    __m3c_waiter_destroy(descs[idx]->waiter);
    free(descs[idx]);
    descs[idx] = nullptr;
    return 0;
}
