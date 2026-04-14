
---

## 项目关键实现讲解

### 一、整体架构

```
┌─────────────────────────────────────────┐
│           MainReactor（主线程）          │
│  - 监听端口                             │
│  - 接受新连接                           │
│  - 分配给 SubReactor                    │
└─────────────────────────────────────────┘
                    │
                    ▼ 轮询分配
┌─────────────────────────────────────────┐
│      SubReactor Pool（IO线程池）         │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐   │
│  │Thread 0 │ │Thread 1 │ │Thread N │   │
│  │EventLoop│ │EventLoop│ │EventLoop│   │
│  │管理连接  │ │管理连接  │ │管理连接  │   │
│  └─────────┘ └─────────┘ └─────────┘   │
└─────────────────────────────────────────┘
```

### 二、核心组件详解

#### 1. EventLoop - 事件循环
```cpp
class EventLoop {
    // 每个线程一个 EventLoop
    void loop() {
        while (!quit_) {
            // 1. epoll_wait 等待事件
            auto channels = epoll_->poll(timeout);
            
            // 2. 处理就绪事件
            for (auto* ch : channels) {
                ch->handleEvent();
            }
            
            // 3. 执行待处理回调
            doPendingFunctors();
        }
    }
};
```

**关键点**：
- `thread_local EventLoop* t_loopInThisThread` - 每个线程只有一个 EventLoop
- `eventfd` - 用于线程间唤醒
- `runInLoop()` / `queueInLoop()` - 线程安全的任务投递

#### 2. Channel - 事件通道
```cpp
class Channel {
    int fd_;                    // 文件描述符
    uint32_t events_;           // 关注的事件（读/写）
    uint32_t revents_;          // 就绪的事件
    
    void handleEvent() {
        if (revents_ & EPOLLIN)  readCallback_();
        if (revents_ & EPOLLOUT) writeCallback_();
    }
};
```

**关键点**：
- 一个 Channel 对应一个 fd
- 回调函数由用户设置
- `update()` 将 Channel 注册到 epoll

#### 3. Acceptor - 连接接收器
```cpp
class Acceptor {
    void handleRead() {
        int connfd = ::accept4(listenFd_, ...);
        if (newConnectionCallback_) {
            newConnectionCallback_(connfd);
        }
    }
};
```

**关键点**：
- 只在 MainReactor 中运行
- 接受连接后立即分配给 SubReactor
- `idleFd_` 处理文件描述符耗尽

#### 4. TcpConnection - 连接管理
```cpp
class TcpConnection {
    void handleRead() {
        int n = ::read(fd_, buf, sizeof(buf));
        if (n > 0) {
            inputBuffer_.append(buf, n);
            messageCallback_(shared_from_this(), &inputBuffer_);
        } else if (n == 0) {
            handleClose();  // 对端关闭
        }
    }
    
    void send(const std::string& data) {
        if (loop_->isInLoopThread()) {
            sendInLoop(data);
        } else {
            loop_->queueInLoop([this, data] { sendInLoop(data); });
        }
    }
};
```

**关键点**：
- 线程安全的发送：`send()` 可以在任意线程调用
- 使用 Buffer 处理粘包
- `enableWriting()` 触发写事件

#### 5. EventLoopThreadPool - IO线程池
```cpp
class EventLoopThreadPool {
    EventLoop* getNextLoop() {
        // 轮询算法
        EventLoop* loop = loops_[next_];
        next_ = (next_ + 1) % loops_.size();
        return loop;
    }
};
```

**关键点**：
- 轮询分配连接，实现负载均衡
- 每个线程独立运行 EventLoop
- 无锁设计，每个连接只在一个线程处理

### 三、执行流程

```
1. 启动服务器
   TcpServer::start()
   ├── EventLoopThreadPool::start()  // 创建IO线程
   └── Acceptor::listen()            // 开始监听

2. 新连接到来（MainReactor）
   Acceptor::handleRead()
   ├── accept() 接受连接
   └── TcpServer::newConnection()
       ├── getNextLoop() 选择SubReactor
       ├── 创建 TcpConnection
       └── ioLoop->runInLoop(connectEstablished)

3. 数据到来（SubReactor）
   Channel::handleEvent()
   └── TcpConnection::handleRead()
       ├── read() 读取数据
       ├── 存入 inputBuffer_
       └── messageCallback_() 回调用户

4. 发送数据（任意线程）
   TcpConnection::send()
   ├── 如果在IO线程：直接发送
   └── 否则：queueInLoop() 投递到IO线程
```

### 四、线程安全设计

| 操作 | 线程 | 说明 |
|-----|------|------|
| 接受连接 | MainReactor | 单线程，无锁 |
| 分配连接 | MainReactor | 轮询算法，无锁 |
| 读写数据 | SubReactor | 每个连接绑定一个线程，无锁 |
| 发送数据 | 任意线程 | 通过 queueInLoop 切换到IO线程 |
| 关闭连接 | SubReactor | 回调到 MainReactor 移除连接 |

### 五、编译运行

```bash
# 编译
cd simple_reactor
make

# 运行（4个IO线程）
./main_reactor_server 8080 4



### 六、关键设计模式

1. **Reactor模式**：事件驱动，非阻塞IO
2. **主从Reactor**：主线程接受连接，IO线程处理数据
3. **线程池**：IO线程池 + 工作线程池分离
4. **RAII**：Socket、Channel自动管理资源
5. **回调机制**：用户通过回调函数处理业务

---

## 文件清单

```
simple_reactor/
├── include/
│   ├── Buffer.h              # 缓冲区
│   ├── ThreadPool.h          # 工作线程池
│   ├── Epoll.h               # epoll封装
│   ├── Channel.h             # 事件通道
│   ├── EventLoop.h           # 事件循环
│   ├── Acceptor.h            # 连接接收器
│   ├── TcpConnection.h       # TCP连接
│   ├── EventLoopThread.h     # IO线程
│   ├── EventLoopThreadPool.h # IO线程池
│   └── TcpServer.h           # TCP服务器
├── src/                      # 实现文件
├── examples/
│   └── main_reactor_server.cpp  # 示例
└── Makefile
```
