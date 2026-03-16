#pragma once

/**
 * @file ThreadPool.h
 * @brief 线程池 - 不阻塞主线程，业务逻辑丢给子线程
 */

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads);
    ~ThreadPool();

    // 添加任务到线程池
    void addTask(std::function<void()> task);

    // 停止线程池
    void stop();

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_;

    void workerLoop();
};
