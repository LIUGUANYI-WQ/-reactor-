#include "TcpServer.h"
#include "EventLoop.h"
#include "Acceptor.h"
#include <sstream>
#include <iostream>

TcpServer::TcpServer(EventLoop* loop, int port)
    : loop_(loop),
      acceptor_(std::make_unique<Acceptor>(loop, port)),
      threadPool_(std::make_unique<EventLoopThreadPool>(loop, "IOThread")),
      started_(false),
      nextConnId_(1) {

    acceptor_->setNewConnectionCallback(
        [this](int sockfd) { this->newConnection(sockfd); });
}

TcpServer::~TcpServer() {
    loop_->runInLoop([this] {
        for (auto& item : connections_) {
            TcpConnectionPtr conn(item.second);
            item.second.reset();
            conn->getLoop()->runInLoop(
                [conn] { conn->connectDestroyed(); });
        }
    });
}

void TcpServer::setThreadNum(int numThreads) {
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::start() {
    if (started_.exchange(true)) {
        return;
    }

    // 启动IO线程池（SubReactor）
    threadPool_->start();

    // 开始监听
    loop_->runInLoop([this] { acceptor_->listen(); });
}

void TcpServer::newConnection(int sockfd) {
    // 在主线程中接受新连接

    // 选择一个SubReactor（轮询）
    EventLoop* ioLoop = threadPool_->getNextLoop();

    // 创建连接名称
    std::ostringstream oss;
    oss << "conn-" << nextConnId_++;
    std::string connName = oss.str();

    std::cout << "New connection [" << connName << "] on "
              << ioLoop << std::endl;

    // 创建TcpConnection
    TcpConnectionPtr conn = std::make_shared<TcpConnection>(ioLoop, sockfd);

    // 设置回调
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback(
        [this](const TcpConnectionPtr& connection) {
            this->removeConnection(connection);
        });

    // 保存连接
    {
        std::lock_guard<std::mutex> lock(mutex_);
        connections_[connName] = conn;
    }

    // 在IO线程中建立连接
    ioLoop->runInLoop([conn] { conn->connectEstablished(); });

    // 通知连接建立
    if (connectionCallback_) {
        connectionCallback_(conn);
    }
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn) {
    // 在主线程中移除连接
    loop_->runInLoop([this, conn] { this->removeConnectionInLoop(conn); });
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn) {
    // 确保在主线程中执行
    bool removed = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = connections_.begin(); it != connections_.end(); ++it) {
            if (it->second == conn) {
                connections_.erase(it);
                removed = true;
                break;
            }
        }
    }

    if (removed) {
        std::cout << "Connection removed" << std::endl;

        // 通知连接关闭
        if (connectionCallback_) {
            connectionCallback_(conn);
        }

        // 在IO线程中销毁连接
        EventLoop* ioLoop = conn->getLoop();
        ioLoop->queueInLoop([conn] { conn->connectDestroyed(); });
    }
}
