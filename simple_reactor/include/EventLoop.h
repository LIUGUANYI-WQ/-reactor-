#pragma once

/**
 * @file EventLoop.h
 * @brief 事件循环 - 主线程死循环，调用 epoll_wait 等待事件就绪
 */

#include <functional>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <queue>

#include "Epoll.h"
#include <thread>

class Channel;

class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    // 启动事件循环
    void loop();

    // 停止事件循环
    void quit();

    // 更新Channel
    void updateChannel(Channel* channel);

    // 移除Channel
    void removeChannel(Channel* channel);

    // 检查是否在IO线程
    bool isInLoopThread() const;

    // 在IO线程中执行回调
    void runInLoop(std::function<void()> cb);

    // 将回调加入队列
    void queueInLoop(std::function<void()> cb);

private:
    void doPendingFunctors();
    void wakeup();
    void handleRead();

    std::unique_ptr<Epoll> epoll_;
    std::thread::id threadId_;
    std::atomic<bool> looping_;
    std::atomic<bool> quit_;

    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_;

    mutable std::mutex mutex_;    std::vector<std::function<void()>> pendingFunctors_;    std::vector<Channel*> activeChannels_;
    std::atomic<bool> callingPendingFunctors_;
};