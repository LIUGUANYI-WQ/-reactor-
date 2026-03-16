#include "Channel.h"
#include "EventLoop.h"
#include <sys/epoll.h>
Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop),
      fd_(fd),
      events_(0),
      revents_(0),
      inEpoll_(false) {
}

Channel::~Channel() {
}

void Channel::handleEvent() {
    // 处理关闭事件
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
        if (closeCallback_) closeCallback_();
    }

    // 处理错误事件
    // if (revents_ & (EPOLLERR |EPOLLNVAL)) {
    //     if (errorCallback_) errorCallback_();
    // }

    // 处理读事件
    if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
        if (readCallback_) readCallback_();
    }

    // 处理写事件
    if (revents_ & EPOLLOUT) {
        if (writeCallback_) writeCallback_();
    }
}

void Channel::update() {
    if (loop_) {
        loop_->updateChannel(this);
    }
}

void Channel::remove() {
    if (loop_) {
        loop_->removeChannel(this);
    }
}