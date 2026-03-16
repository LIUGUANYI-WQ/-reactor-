#include "EventLoop.h"
#include "Channel.h"
#include<thread>
#include <sys/eventfd.h>
#include <unistd.h>
#include <assert.h>
#include <iostream>
using namespace std;
thread_local EventLoop* t_loopInThisThread = nullptr;

EventLoop::EventLoop()
    : epoll_(std::make_unique<Epoll>()),
      threadId_(this_thread::get_id()),
      looping_(false),
      quit_(false),
      wakeupFd_(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) {

    if (wakeupFd_ < 0) {
        std::cerr << "eventfd create failed" << std::endl;
    }

    wakeupChannel_ = std::make_unique<Channel>(this, wakeupFd_);
    wakeupChannel_->setReadCallback([this] { handleRead(); });
    wakeupChannel_->enableReading();

    if (t_loopInThisThread) {
        std::cerr << "Another EventLoop exists in this thread" << std::endl;
    } else {
        t_loopInThisThread = this;
    }
}

EventLoop::~EventLoop() {
    assert(!looping_);
    quit_ = true;
    
    // 如果当前不在loop线程，则不能直接操作wakeupChannel_
    if (isInLoopThread()) {
        wakeupChannel_->disableAll();
        wakeupChannel_->remove();
    }
    
    close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::loop() {
    if (looping_) {
        std::cerr << "EventLoop already looping" << std::endl;
        return;
    }

    assert(!quit_);
    looping_ = true;
    quit_ = false;

    while (!quit_) {
        activeChannels_.clear();
        std::vector<Channel*> activeChannels = epoll_->poll(10000);
        activeChannels_ = activeChannels;

        for (Channel* channel : activeChannels_) {
            channel->handleEvent();
        }

        doPendingFunctors();
    }

    looping_ = false;
}

void EventLoop::quit() {
    quit_ = true;
    if (!isInLoopThread()) {
        wakeup();
    }
}

void EventLoop::updateChannel(Channel* channel) {
    if (epoll_) {
        epoll_->updateChannel(channel);
    }
}

void EventLoop::removeChannel(Channel* channel) {
    if (epoll_) {
        epoll_->removeChannel(channel);
    }
}

bool EventLoop::isInLoopThread() const {
    return std::this_thread::get_id() == threadId_;
}

void EventLoop::runInLoop(std::function<void()> cb) {
    if (isInLoopThread()) {
        cb();
    } else {
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(std::function<void()> cb) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingFunctors_.push_back(std::move(cb));
    }

    // 只有当不在loop线程中，或者正在调用pending函数时才唤醒
    if (!isInLoopThread() || callingPendingFunctors_) {
        wakeup();
    }
}

void EventLoop::doPendingFunctors() {
    std::vector<std::function<void()>> functors;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    callingPendingFunctors_ = true;

    for (const auto& functor : functors) {
        if (functor) {
            functor();
        }
    }

    callingPendingFunctors_ = false;
}

void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        std::cerr << "wakeup write error: " << errno << std::endl;
    }
}

void EventLoop::handleRead() {
    uint64_t one = 0;
    ssize_t n = read(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        std::cerr << "wakeup read error: " << errno << std::endl;
    }
}