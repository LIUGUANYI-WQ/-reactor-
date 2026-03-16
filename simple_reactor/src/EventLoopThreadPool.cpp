#include "EventLoopThreadPool.h"
#include "EventLoop.h"
#include <iostream>

EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg)
    : baseLoop_(baseLoop),
      name_(nameArg),
      started_(false),
      numThreads_(0),
      next_(0) {
}

EventLoopThreadPool::~EventLoopThreadPool() {
    // EventLoopThread的析构函数会停止线程
}

void EventLoopThreadPool::start(const ThreadInitCallback& cb) {
    if (started_) {
        return;
    }

    started_ = true;

    // 创建指定数量的IO线程
    for (int i = 0; i < numThreads_; ++i) {
        char buf[name_.size() + 32];
        snprintf(buf, sizeof(buf), "%s%d", name_.c_str(), i);

        EventLoopThread* t = new EventLoopThread(cb, buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        loops_.push_back(t->startLoop());
    }

    // 如果没有子线程，直接使用baseLoop
    if (numThreads_ == 0 && cb) {
        cb(baseLoop_);
    }
}

EventLoop* EventLoopThreadPool::getNextLoop() {
    // 如果没有子线程，返回主EventLoop
    if (loops_.empty()) {
        return baseLoop_;
    }

    // 轮询选择
    EventLoop* loop = loops_[next_];
    ++next_;
    if (next_ >= loops_.size()) {
        next_ = 0;
    }

    return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops() {
    if (loops_.empty()) {
        return std::vector<EventLoop*>(1, baseLoop_);
    } else {
        return loops_;
    }
}
