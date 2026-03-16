#include "Epoll.h"
#include "Channel.h"
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <iostream>

Epoll::Epoll()
    : epollFd_(-1),
      events_(kMaxEvents) {
    epollFd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epollFd_ < 0) {
        std::cerr << "epoll_create1 failed: " << strerror(errno) << std::endl;
    }
}

Epoll::~Epoll() {
    if (epollFd_ >= 0) {
        close(epollFd_);
    }
}

void Epoll::updateChannel(Channel* channel) {
    if (!channel) return;

    epoll_event event;
    memset(&event, 0, sizeof(event));
    event.events = channel->events();
    event.data.ptr = channel;

    int fd = channel->fd();
    int op = channel->isInEpoll() ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;

    if (epoll_ctl(epollFd_, op, fd, &event) < 0) {
        std::cerr << "epoll_ctl failed: " << strerror(errno) << std::endl;
        return;
    }

    channel->setInEpoll(true);
}

void Epoll::removeChannel(Channel* channel) {
    if (!channel || !channel->isInEpoll()) return;

    int fd = channel->fd();
    if (epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
        std::cerr << "epoll_ctl DEL failed: " << strerror(errno) << std::endl;
    }

    channel->setInEpoll(false);
}

std::vector<Channel*> Epoll::poll(int timeoutMs) {
    std::vector<Channel*> activeChannels;
    activeChannels.reserve(kMaxEvents);

    int numEvents = epoll_wait(epollFd_, events_.data(), static_cast<int>(events_.size()), timeoutMs);

    if (numEvents < 0) {
        if (errno != EINTR) {
            std::cerr << "epoll_wait failed: " << strerror(errno) << std::endl;
        }
        return activeChannels;
    }

    for (int i = 0; i < numEvents; ++i) {
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
        if (channel) {
            channel->setRevents(events_[i].events);
            activeChannels.push_back(channel);
        }
    }

    return activeChannels;
}
