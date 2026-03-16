# 主从Reactor模型 Makefile

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread
INCLUDES = -I./include

# 源文件
SRCS = src/Buffer.cpp \
       src/ThreadPool.cpp \
       src/Epoll.cpp \
       src/Channel.cpp \
       src/EventLoop.cpp \
       src/Acceptor.cpp \
       src/TcpConnection.cpp \
       src/EventLoopThread.cpp \
       src/EventLoopThreadPool.cpp \
       src/TcpServer.cpp

# 对象文件
OBJS = $(SRCS:.cpp=.o)

# 目标
TARGET = main_reactor_server

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS) examples/main_reactor_server.o
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) examples/*.o $(TARGET)

# 运行示例（4个IO线程）
run: $(TARGET)
	./$(TARGET) 8080 4

# 单线程模式（用于对比测试）
run-single: $(TARGET)
	./$(TARGET) 8080 0

# 8线程模式（高并发）
run-multi: $(TARGET)
	./$(TARGET) 8080 8
