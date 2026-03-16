#pragma once

/**
 * @file TcpServer.h
 * @brief TCP服务器 - 主从Reactor模型实现
 * 
 * 架构：
 * - 主线程（MainReactor）：负责监听端口、接受新连接
 * - IO线程池（SubReactor）：负责处理已连接socket的读写事件
 * - 工作线程池：负责业务逻辑处理
 */

#include <functional>
#include <memory>
#include <map>
#include <mutex>
#include <atomic>

#include "EventLoopThreadPool.h"
#include "TcpConnection.h"

class EventLoop;
class Acceptor;

class TcpServer {
public:
    using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
    using MessageCallback = std::function<void(const TcpConnectionPtr&, Buffer*)>;
    using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;

    TcpServer(EventLoop* loop, int port);
    ~TcpServer();

    // 设置线程数（IO线程数，即SubReactor数量）
    void setThreadNum(int numThreads);

    // 启动服务器
    void start();

    // 设置回调
    void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }

private:
    // 新连接回调（在主线程中调用）
    void newConnection(int sockfd);

    // 连接关闭回调
    void removeConnection(const TcpConnectionPtr& conn);

    // 在IO线程中移除连接
    void removeConnectionInLoop(const TcpConnectionPtr& conn);

    EventLoop* loop_;  // 主EventLoop（MainReactor）
    std::unique_ptr<Acceptor> acceptor_;
    std::unique_ptr<EventLoopThreadPool> threadPool_;  // IO线程池（SubReactor）

    std::atomic<bool> started_;
    std::atomic<int> nextConnId_;

    // 连接管理
    std::map<std::string, TcpConnectionPtr> connections_;
    std::mutex mutex_;

    // 回调
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
};
