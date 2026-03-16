/**
 * @file echo_server.cpp
 * @brief 回显服务器示例 - 演示Reactor网络库的使用
 *
 * Reactor执行流程：
 * 1. 主线程创建 epoll 文件描述符
 * 2. 创建监听 socket，注册读事件到 epoll
 * 3. 进入事件循环：epoll_wait 阻塞等待就绪
 * 4. 新连接就绪：Acceptor 接收连接，将新 socket 加入 epoll
 * 5. 已连接 socket 就绪：
 *    - 读就绪 → 调用 read 读取数据
 *    - 数据读完 → 抛给线程池处理业务
 *    - 业务处理完 → 写就绪 → 调用 write 回写数据
 * 6. 循环往复
 */

#include <iostream>
#include <csignal>
#include <memory>
#include <map>
#include <mutex>

#include "EventLoop.h"
#include "Acceptor.h"
#include "TcpConnection.h"
#include "ThreadPool.h"

// 全局变量
std::unique_ptr<EventLoop> g_loop;
std::map<int, TcpConnectionPtr> g_connections;
std::mutex g_mutex;
ThreadPool g_threadPool(4);  // 4个工作线程

// 信号处理
void signalHandler(int sig) {
    std::cout << "Received signal " << sig << ", shutting down..." << std::endl;
    if (g_loop) {
        g_loop->quit();
    }
}

// 移除连接
void removeConnection(const TcpConnectionPtr& conn) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_connections.erase(conn->fd());
}

// 新连接回调
void onNewConnection(int sockfd) {
    std::cout << "New connection, fd=" << sockfd << std::endl;

    TcpConnectionPtr conn = std::make_shared<TcpConnection>(g_loop.get(), sockfd);

    // 设置消息回调
    conn->setMessageCallback([](const TcpConnectionPtr& conn, Buffer* buffer) {
        // 读取数据
        std::string data = buffer->retrieveAllAsString();
        std::cout << "Received: " << data << std::endl;

        // 将业务逻辑丢给线程池处理
        g_threadPool.addTask([conn, data]() {
            // 业务处理（这里只是简单的回显）
            std::string response = "Echo: " + data;

            // 发送响应（线程安全）
            conn->send(response);
        });
    });

    // 设置关闭回调
    conn->setCloseCallback([](const TcpConnectionPtr& conn) {
        std::cout << "Connection closed, fd=" << conn->fd() << std::endl;
        removeConnection(conn);
    });

    // 保存连接
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_connections[sockfd] = conn;
    }

    // 建立连接
    conn->connectEstablished();
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        return 1;
    }

    int port = std::atoi(argv[1]);

    // 注册信号处理
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // 1. 创建 EventLoop（主线程死循环）
    g_loop = std::make_unique<EventLoop>();

    // 2. 创建 Acceptor（专门监听新连接）
    Acceptor acceptor(g_loop.get(), port);
    acceptor.setNewConnectionCallback(onNewConnection);
    acceptor.listen();

    std::cout << "Echo server listening on port " << port << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;

    // 3. 进入事件循环：epoll_wait 阻塞等待就绪
    g_loop->loop();

    // 清理
    g_threadPool.stop();
    std::cout << "Server stopped" << std::endl;

    return 0;
}
