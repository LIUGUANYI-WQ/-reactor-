# 压力测试指南

## 一、使用自带的压测客户端

### 1. 编译
```bash
cd simple_reactor
mkdir build && cd build
cmake ..
make -j
```

### 2. 启动服务器
```bash
# 终端1：启动服务器（4个IO线程）
./bin/main_reactor_server 8080 4
```

### 3. 运行压测客户端
```bash
# 终端2：运行压测
./bin/benchmark_client 127.0.0.1 8080 4 1000 30

# 参数说明：
# 127.0.0.1    - 服务器IP
# 8080         - 服务器端口
# 4            - 客户端线程数
# 1000         - 每线程连接数（总连接数 = 4 * 1000 = 4000）
# 30           - 测试时长（秒）
```

### 4. 输出示例
```
========================================
      压力测试客户端启动
========================================
目标服务器: 127.0.0.1:8080
客户端线程: 4
每线程连接: 1000
总连接数:   4000
测试时长:   30 秒
========================================
[1s] Connections: 4000 | Sent: 12000 | Recv: 12000 | Throughput: 2 MB | Errors: 0 | Avg Latency: 0.5 ms
[2s] Connections: 4000 | Sent: 25000 | Recv: 25000 | Throughput: 4 MB | Errors: 0 | Avg Latency: 0.4 ms
...
========================================
           测试完成 - 最终统计
========================================
总连接数:     4000
发送消息:     350000
接收消息:     350000
发送字节:     21000000 (20 MB)
接收字节:     21000000 (20 MB)
错误数:       0
平均QPS:      11666.7
平均延迟:     0.45 ms
========================================
```

---

## 二、使用 tcpkali 压测工具

### 安装 tcpkali
```bash
# Ubuntu/Debian
sudo apt-get install tcpkali

# macOS
brew install tcpkali

# 源码编译
git clone https://github.com/satori-com/tcpkali
cd tcpkali && ./configure && make && sudo make install
```

### 基本压测命令

#### 1. 测试并发连接数
```bash
# 创建 10000 个并发连接，每秒新建 1000 个
tcpkali -c 10000 --connect-rate 1000 127.0.0.1:8080
```

#### 2. 测试吞吐量
```bash
# 100 个连接，持续 30 秒，发送指定消息
tcpkali -c 100 -T 30s -m "Hello Server\n" 127.0.0.1:8080
```

#### 3. 测试双向流量
```bash
# 1000 个连接，持续 60 秒
tcpkali -c 1000 -T 60s \
    -m "REQUEST\n" \
    -em "RESPONSE\n" \
    127.0.0.1:8080
```

#### 4. 极限压力测试
```bash
# 高并发 + 高频率发送
tcpkali -c 5000 \
    --channel-bandwidth 10M \
    -T 60s \
    127.0.0.1:8080
```

### tcpkali 参数说明

| 参数 | 说明 |
|-----|------|
| `-c, --connections` | 并发连接数 |
| `-T, --duration` | 测试时长 |
| `-m, --message` | 发送的消息内容 |
| `-em, --echo-message` | 期望接收的响应 |
| `--connect-rate` | 每秒新建连接数 |
| `--channel-bandwidth` | 每个连接的带宽 |
| `-w, --workers` | 工作线程数 |

---

## 三、对比测试方案

### 测试 1：单线程 vs 主从Reactor
```bash
# 终端1：单线程模式
./bin/main_reactor_server 8080 0

# 终端2：压测
./bin/benchmark_client 127.0.0.1 8080 4 100 30

# 记录结果，然后测试主从模式
./bin/main_reactor_server 8080 4
./bin/benchmark_client 127.0.0.1 8080 4 100 30
```

### 测试 2：不同IO线程数对比
```bash
# 1个IO线程
./bin/main_reactor_server 8080 1 &
./bin/benchmark_client 127.0.0.1 8080 4 1000 30
kill %1

# 4个IO线程
./bin/main_reactor_server 8080 4 &
./bin/benchmark_client 127.0.0.1 8080 4 1000 30
kill %1

# 8个IO线程
./bin/main_reactor_server 8080 8 &
./bin/benchmark_client 127.0.0.1 8080 4 1000 30
kill %1
```

### 测试 3：连接数梯度测试
```bash
# 100 连接
./bin/benchmark_client 127.0.0.1 8080 4 25 30

# 1000 连接
./bin/benchmark_client 127.0.0.1 8080 4 250 30

# 10000 连接
./bin/benchmark_client 127.0.0.1 8080 4 2500 30
```

---

## 四、性能指标说明

| 指标 | 说明 | 优秀标准 |
|-----|------|---------|
| QPS | 每秒处理请求数 | > 10000 |
| 延迟 | 平均响应时间 | < 1ms |
| 错误率 | 失败请求比例 | < 0.1% |
| 吞吐量 | 每秒传输数据量 | > 100MB/s |
| 并发连接 | 同时保持的连接数 | > 10000 |

---

## 五、系统调优建议

### 1. 修改文件描述符限制
```bash
# 查看当前限制
ulimit -n

# 临时修改
ulimit -n 65535

# 永久修改（/etc/security/limits.conf）
* soft nofile 65535
* hard nofile 65535
```

### 2. 修改 TCP 参数
```bash
# /etc/sysctl.conf
net.ipv4.tcp_tw_reuse = 1
net.ipv4.tcp_tw_recycle = 0
net.ipv4.tcp_fin_timeout = 30
net.ipv4.tcp_keepalive_time = 1200
net.ipv4.ip_local_port_range = 1024 65535
net.ipv4.tcp_max_syn_backlog = 8192
net.ipv4.tcp_max_tw_buckets = 5000
net.core.somaxconn = 8192
net.core.netdev_max_backlog = 65536

# 生效
sysctl -p
```

### 3. 关闭防火墙（测试环境）
```bash
# Ubuntu
sudo ufw disable

# CentOS
sudo systemctl stop firewalld
```

---

## 六、常见问题

### Q1: 连接数达到上限
**现象**：无法创建更多连接
**解决**：
```bash
# 增加文件描述符限制
ulimit -n 65535

# 检查系统连接数
cat /proc/sys/net/netfilter/nf_conntrack_max
```

### Q2: CPU 使用率低
**现象**：QPS上不去，CPU没跑满
**原因**：IO线程数不足或客户端压力不够
**解决**：
- 增加服务器IO线程数
- 增加客户端线程数和连接数

### Q3: 延迟过高
**现象**：平均延迟 > 10ms
**原因**：
- 业务处理耗时
- 网络延迟
- 系统负载高
**解决**：
- 优化业务逻辑
- 使用工作线程池异步处理
- 检查系统负载

---

## 七、压测脚本

创建自动化压测脚本：

```bash
#!/bin/bash
# benchmark.sh

SERVER_IP="127.0.0.1"
SERVER_PORT="8080"
IO_THREADS=(1 2 4 8)
CLIENT_THREADS=4
CONNECTIONS_PER_THREAD=250
DURATION=30

echo "开始压测..."

for io_thread in "${IO_THREADS[@]}"; do
    echo "========================================"
    echo "测试 IO 线程数: $io_thread"
    echo "========================================"
    
    # 启动服务器
    ./bin/main_reactor_server $SERVER_PORT $io_thread &
    SERVER_PID=$!
    sleep 2
    
    # 运行压测
    ./bin/benchmark_client $SERVER_IP $SERVER_PORT \
        $CLIENT_THREADS $CONNECTIONS_PER_THREAD $DURATION \
        > result_${io_thread}threads.txt
    
    # 停止服务器
    kill $SERVER_PID
    wait $SERVER_PID 2>/dev/null
    
    echo "完成 $io_thread 线程测试"
    echo ""
done

echo "所有测试完成，结果保存在 result_*.txt"
```

运行：
```bash
chmod +x benchmark.sh
./benchmark.sh
```
