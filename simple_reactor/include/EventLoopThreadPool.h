#pragma once

/**
 * @file EventLoopThreadPool.h
 * @brief IO线程池 - 管理多个SubReactor
 * 
 * 主从Reactor模型：
 * - MainReactor：主线程，负责监听和接受新连接
 * - SubReactor：IO线程池，负责处理已连接socket的IO事件
 */

#include <vector>
#include <memory>
#include <functional>
#include <string>

#include "EventLoopThread.h"

class EventLoop;

class EventLoopThreadPool {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg);
    ~EventLoopThreadPool();

    // 设置线程数
    void setThreadNum(int numThreads) { numThreads_ = numThreads; }

    // 启动线程池
    void start(const ThreadInitCallback& cb = ThreadInitCallback());

    // 获取下一个EventLoop（轮询）
    EventLoop* getNextLoop();

    // 获取所有EventLoop
    std::vector<EventLoop*> getAllLoops();

    // 是否已启动
    bool started() const { return started_; }

    // 获取名称
    const std::string& name() const { return name_; }

private:
    EventLoop* baseLoop_;  // 主EventLoop（MainReactor）
    std::string name_;
    bool started_;
    int numThreads_;
    size_t next_;
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop*> loops_;
};
