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

#include <base/Common.h>

#include <m3/vfs/File.h>
#include <m3/vfs/FileTable.h>
#include <m3/vfs/Waiter.h>

#include <errno.h>
#include <sys/epoll.h>

#include "base/time/Duration.h"
#include "intern.h"

constexpr size_t MAX_EPOLL_DESCS = 4;
constexpr size_t MAX_EPOLL_FILES = 8;

struct EPollDesc {
    explicit EPollDesc() : waiter(), data() {
        for(size_t i = 0; i < MAX_EPOLL_FILES; ++i)
            data[i].fd = -1;
    }

    m3::FileWaiter waiter;
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
            descs[i] = new EPollDesc();
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
        case EPOLL_CTL_ADD: desc->waiter.add(fd, events); break;
        case EPOLL_CTL_MOD: desc->waiter.set(fd, events); break;
        case EPOLL_CTL_DEL: desc->waiter.remove(fd); break;
    }

    return 0;
}

EXTERN_C int __m3_epoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout,
                              const sigset_t *) {
    EPollDesc *desc = get_desc(epfd);
    if(!desc)
        return -EBADF;

    if(timeout == -1)
        desc->waiter.wait();
    else {
        // for 0 (= return immediately), we use a very short timeout to check once and count the
        // ready file descriptors
        auto m3_timeout =
            timeout == 0
                ? m3::TimeDuration::from_nanos(1)
                : m3::TimeDuration::from_millis(static_cast<m3::TimeDuration::raw_t>(timeout));
        desc->waiter.wait_for(m3_timeout);
    }

    int idx = 0;
    desc->waiter.foreach_ready([desc, events, &idx, maxevents](int fd, uint fdevs) {
        if(idx < maxevents) {
            events[idx].events = 0;
            if(fdevs & m3::File::INPUT)
                events[idx].events |= EPOLLIN;
            if(fdevs & m3::File::OUTPUT)
                events[idx].events |= EPOLLOUT;
            for(size_t i = 0; i < MAX_EPOLL_FILES; ++i) {
                if(desc->data[i].fd == fd) {
                    memcpy(&events[idx].data, &desc->data[i].data, sizeof(epoll_data));
                    break;
                }
            }
            idx++;
        }
    });
    return idx;
}

EXTERN_C int __m3_epoll_close(int epfd) {
    EPollDesc *desc = get_desc(epfd);
    if(!desc)
        return -EBADF;

    int idx = epfd - m3::FileTable::MAX_FDS;
    delete descs[idx];
    descs[idx] = nullptr;
    return 0;
}
