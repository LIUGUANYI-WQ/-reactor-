#pragma once

/**
 * @file EventLoopThread.h
 * @brief IO线程封装 - 每个线程运行一个EventLoop
 * 
 * 主从Reactor模型中的SubReactor
 */

#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <string>

class EventLoop;

class EventLoopThread {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(),
                    const std::string& name = std::string());
    ~EventLoopThread();

    // 启动线程，返回EventLoop指针
    EventLoop* startLoop();

    // 获取线程名称
    const std::string& name() const { return name_; }

private:
    void threadFunc();

    EventLoop* loop_;
    bool exiting_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;
    std::string name_;
};
