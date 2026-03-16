/**
 * @file main_reactor_server.cpp
 * @brief 主从Reactor模型示例
 *
 * 架构说明：
 * 1. MainReactor（主线程）：负责监听端口、接受新连接
 * 2. SubReactor（IO线程池）：负责处理已连接socket的读写事件
 * 3. WorkerThreadPool（工作线程池）：负责业务逻辑处理
 *
 * 执行流程：
 * 1. 主线程创建 epoll，监听端口
 * 2. 启动IO线程池（SubReactor），每个线程有自己的epoll
 * 3. 新连接到来：MainReactor接受连接，分配给SubReactor
 * 4. SubReactor处理该连接的所有IO事件（读/写）
 * 5. 业务逻辑丢给工作线程池处理
 */

#include <iostream>
#include <csignal>
#include <memory>

#include "EventLoop.h"
#include "TcpServer.h"
#include "ThreadPool.h"

// 全局变量
std::unique_ptr<EventLoop> g_mainLoop;
std::unique_ptr<TcpServer> g_server;
std::unique_ptr<ThreadPool> g_workerPool;

// 信号处理
void signalHandler(int sig) {
    std::cout << "\nReceived signal " << sig << ", shutting down..." << std::endl;
    if (g_mainLoop) {
        g_mainLoop->quit();
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <port> <io_threads>" << std::endl;
        std::cerr << "Example: " << argv[0] << " 8080 4" << std::endl;
        std::cerr << "  port:       监听端口" << std::endl;
        std::cerr << "  io_threads: IO线程数（SubReactor数量）" << std::endl;
        return 1;
    }

    int port = std::atoi(argv[1]);
    int ioThreads = std::atoi(argv[2]);

    // 注册信号处理
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "========================================" << std::endl;
    std::cout << "  主从Reactor模型 TCP服务器" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "监听端口: " << port << std::endl;
    std::cout << "IO线程数: " << ioThreads << " (SubReactor)" << std::endl;
    std::cout << "工作线程: 4 (业务处理)" << std::endl;
    std::cout << "========================================" << std::endl;

    // 1. 创建主EventLoop（MainReactor）
    g_mainLoop = std::make_unique<EventLoop>();

    // 2. 创建TCP服务器
    g_server = std::make_unique<TcpServer>(g_mainLoop.get(), port);

    // 3. 设置IO线程数（SubReactor数量）
    g_server->setThreadNum(ioThreads);

    // 4. 创建工作线程池（处理业务逻辑）
    g_workerPool = std::make_unique<ThreadPool>(4);

    // 5. 设置回调函数
    g_server->setConnectionCallback(
        [](const TcpConnectionPtr& conn) {
            std::cout << "[连接事件] fd=" << conn->fd()
                      << " loop=" << conn->getLoop() << std::endl;
        });

    g_server->setMessageCallback(
        [](const TcpConnectionPtr& conn, Buffer* buffer) {
            // 读取数据
            std::string data = buffer->retrieveAllAsString();

            // 将业务逻辑丢给工作线程池
            g_workerPool->addTask([conn, data]() {
                // 模拟业务处理
                std::string response = "Echo[" + std::to_string(conn->fd()) + "]: " + data;

                // 发送响应（线程安全）
                conn->send(response);
            });
        });

    g_server->setWriteCompleteCallback(
        [](const TcpConnectionPtr& conn) {
            std::cout << "[写完成] fd=" << conn->fd() << std::endl;
        });

    // 6. 启动服务器
    g_server->start();

    std::cout << "服务器已启动，按 Ctrl+C 停止" << std::endl;
    std::cout << "========================================" << std::endl;

    // 7. 进入主循环（MainReactor）
    g_mainLoop->loop();

    // 8. 清理资源
    std::cout << "正在清理资源..." << std::endl;
    g_workerPool->stop();
    g_server.reset();
    g_mainLoop.reset();

    std::cout << "服务器已停止" << std::endl;

    return 0;
}
