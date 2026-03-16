#pragma once

/**
 * @file Acceptor.h
 * @brief 专门监听新连接，把新 socket 加入 epoll
 */

#include <functional>
#include <memory>

class EventLoop;
class Channel;

class Acceptor {
public:
    using NewConnectionCallback = std::function<void(int sockfd)>;

    Acceptor(EventLoop* loop, int port);
    ~Acceptor();

    // 设置新连接回调
    void setNewConnectionCallback(const NewConnectionCallback& cb) {
        newConnectionCallback_ = cb;
    }

    // 开始监听
    void listen();

    // 获取监听端口
    int port() const { return port_; }

private:
    void handleRead();

    EventLoop* loop_;
    int port_;
    int listenFd_;
    std::unique_ptr<Channel> acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    int idleFd_;  // 用于处理文件描述符耗尽
};
 