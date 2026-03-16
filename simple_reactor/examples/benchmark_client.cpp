/**
 * @file benchmark_client.cpp
 * @brief 压力测试客户端
 *
 * 功能：
 * 1. 创建大量并发连接
 * 2. 持续发送数据
 * 3. 统计QPS、延迟、错误率
 */

#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

// 统计信息
struct Statistics {
    std::atomic<uint64_t> connections{0};      // 成功连接数
    std::atomic<uint64_t> messagesSent{0};     // 发送消息数
    std::atomic<uint64_t> messagesRecv{0};     // 接收消息数
    std::atomic<uint64_t> bytesSent{0};        // 发送字节数
    std::atomic<uint64_t> bytesRecv{0};        // 接收字节数
    std::atomic<uint64_t> errors{0};           // 错误数
    std::atomic<uint64_t> latency{0};          // 总延迟（微秒）
};

Statistics g_stats;
std::atomic<bool> g_running{true};

// 设置非阻塞
void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// 工作线程
void workerThread(const std::string& ip, int port, int connectionsPerThread, int duration) {
    std::vector<int> fds;
    fds.reserve(connectionsPerThread);

    // 1. 建立所有连接
    for (int i = 0; i < connectionsPerThread; ++i) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            g_stats.errors++;
            continue;
        }

        setNonBlocking(fd);

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

        if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            if (errno != EINPROGRESS) {
                close(fd);
                g_stats.errors++;
                continue;
            }
        }

        fds.push_back(fd);
        g_stats.connections++;
    }

    // 2. 等待所有连接建立
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 3. 持续发送数据
    std::string message = "Hello, Reactor Server! This is a benchmark test message.\n";
    auto startTime = std::chrono::steady_clock::now();

    while (g_running) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
        if (elapsed >= duration) break;

        for (int fd : fds) {
            // 发送数据
            auto sendStart = std::chrono::steady_clock::now();
            ssize_t n = send(fd, message.data(), message.size(), MSG_NOSIGNAL);
            if (n > 0) {
                g_stats.messagesSent++;
                g_stats.bytesSent += n;
            } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                g_stats.errors++;
            }

            // 接收数据（非阻塞）
            char buf[4096];
            n = recv(fd, buf, sizeof(buf), 0);
            if (n > 0) {
                g_stats.messagesRecv++;
                g_stats.bytesRecv += n;
                auto sendEnd = std::chrono::steady_clock::now();
                g_stats.latency += std::chrono::duration_cast<std::chrono::microseconds>(sendEnd - sendStart).count();
            }
        }

        // 防止CPU占用过高
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    // 4. 关闭连接
    for (int fd : fds) {
        close(fd);
    }
}

// 打印统计信息
void printStats(int duration) {
    auto startTime = std::chrono::steady_clock::now();

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
        if (elapsed >= duration) break;

        uint64_t sent = g_stats.messagesSent.load();
        uint64_t recv = g_stats.messagesRecv.load();
        uint64_t bytes = g_stats.bytesSent.load();
        uint64_t errs = g_stats.errors.load();
        uint64_t lat = g_stats.latency.load();

        double avgLatency = (recv > 0) ? (lat / 1000.0 / recv) : 0;

        std::cout << "[" << elapsed << "s] "
                  << "Connections: " << g_stats.connections.load()
                  << " | Sent: " << sent
                  << " | Recv: " << recv
                  << " | Throughput: " << (bytes / 1024 / 1024) << " MB"
                  << " | Errors: " << errs
                  << " | Avg Latency: " << avgLatency << " ms"
                  << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 6) {
        std::cerr << "Usage: " << argv[0] << " <ip> <port> <threads> <connections_per_thread> <duration>" << std::endl;
        std::cerr << "Example: " << argv[0] << " 127.0.0.1 8080 4 1000 30" << std::endl;
        std::cerr << "  ip:                    服务器IP" << std::endl;
        std::cerr << "  port:                  服务器端口" << std::endl;
        std::cerr << "  threads:               客户端线程数" << std::endl;
        std::cerr << "  connections_per_thread: 每线程连接数" << std::endl;
        std::cerr << "  duration:              测试时长(秒)" << std::endl;
        return 1;
    }

    std::string ip = argv[1];
    int port = std::atoi(argv[2]);
    int threads = std::atoi(argv[3]);
    int connectionsPerThread = std::atoi(argv[4]);
    int duration = std::atoi(argv[5]);

    int totalConnections = threads * connectionsPerThread;

    std::cout << "========================================" << std::endl;
    std::cout << "      压力测试客户端启动" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "目标服务器: " << ip << ":" << port << std::endl;
    std::cout << "客户端线程: " << threads << std::endl;
    std::cout << "每线程连接: " << connectionsPerThread << std::endl;
    std::cout << "总连接数:   " << totalConnections << std::endl;
    std::cout << "测试时长:   " << duration << " 秒" << std::endl;
    std::cout << "========================================" << std::endl;

    // 启动工作线程
    std::vector<std::thread> workers;
    for (int i = 0; i < threads; ++i) {
        workers.emplace_back(workerThread, ip, port, connectionsPerThread, duration);
    }

    // 启动统计线程
    std::thread statsThread(printStats, duration);

    // 等待测试完成
    std::this_thread::sleep_for(std::chrono::seconds(duration));
    g_running = false;

    // 等待所有线程结束
    for (auto& t : workers) {
        t.join();
    }
    statsThread.join();

    // 打印最终统计
    std::cout << "========================================" << std::endl;
    std::cout << "           测试完成 - 最终统计" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "总连接数:     " << g_stats.connections.load() << std::endl;
    std::cout << "发送消息:     " << g_stats.messagesSent.load() << std::endl;
    std::cout << "接收消息:     " << g_stats.messagesRecv.load() << std::endl;
    std::cout << "发送字节:     " << g_stats.bytesSent.load() << " (" << (g_stats.bytesSent.load() / 1024 / 1024) << " MB)" << std::endl;
    std::cout << "接收字节:     " << g_stats.bytesRecv.load() << " (" << (g_stats.bytesRecv.load() / 1024 / 1024) << " MB)" << std::endl;
    std::cout << "错误数:       " << g_stats.errors.load() << std::endl;

    double qps = g_stats.messagesRecv.load() / (double)duration;
    double avgLatency = (g_stats.messagesRecv.load() > 0) ?
                        (g_stats.latency.load() / 1000.0 / g_stats.messagesRecv.load()) : 0;

    std::cout << "平均QPS:      " << qps << std::endl;
    std::cout << "平均延迟:     " << avgLatency << " ms" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
