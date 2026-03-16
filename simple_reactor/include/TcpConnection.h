#pragma once

/**
 * @file TcpConnection.h
 * @brief TCP连接类 - 管理单个连接
 */

#include <functional>
#include <memory>
#include <string>
#include "Buffer.h"

class EventLoop;
class Channel;

class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    using MessageCallback = std::function<void(const std::shared_ptr<TcpConnection>&, Buffer*)>;
    using CloseCallback = std::function<void(const std::shared_ptr<TcpConnection>&)>;
    using WriteCompleteCallback = std::function<void(const std::shared_ptr<TcpConnection>&)>;

    TcpConnection(EventLoop* loop, int sockfd);
    ~TcpConnection();

    // 连接建立
    void connectEstablished();
    
    // 连接销毁
    void connectDestroyed();
    


    // 发送数据
    void send(const std::string& data);

    // 关闭连接
    void shutdown();

    // 设置回调
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
    void setCloseCallback(const CloseCallback& cb) { closeCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }

    // 获取文件描述符
    int fd() const { return sockfd_; }

    // 获取所属EventLoop
    EventLoop* getLoop() const { return loop_; }

private:
    void handleRead();
    void handleWrite();
    void handleClose();
    void handleError();
    void sendInLoop(const std::string& data);

    EventLoop* loop_;
    int sockfd_;
    std::unique_ptr<Channel> channel_;

    Buffer inputBuffer_;
    Buffer outputBuffer_;

    MessageCallback messageCallback_;
    CloseCallback closeCallback_;
    WriteCompleteCallback writeCompleteCallback_;
};

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;