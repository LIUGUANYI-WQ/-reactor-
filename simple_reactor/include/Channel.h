#pragma once

/**
 * @file Channel.h
 * @brief 事件处理器 - 每个 socket 绑定：回调函数 + 事件类型（读 / 写 / 异常）
 */

#include <functional>
#include <sys/epoll.h>

class EventLoop;

class Channel {
public:
    // 事件类型
    static const uint32_t kNoneEvent = 0;
    static const uint32_t kReadEvent = EPOLLIN | EPOLLPRI;
    static const uint32_t kWriteEvent = EPOLLOUT;
    static const uint32_t kErrorEvent = EPOLLERR;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    // 处理事件
    void handleEvent();

    // 设置回调函数
    void setReadCallback(std::function<void()> cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(std::function<void()> cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(std::function<void()> cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(std::function<void()> cb) { errorCallback_ = std::move(cb); }

    // 启用/禁用事件
    void enableReading() { events_ |= kReadEvent; update(); }
    void disableReading() { events_ &= ~kReadEvent; update(); }
    void enableWriting() { events_ |= kWriteEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    void disableAll() { events_ = kNoneEvent; update(); }

    // 获取文件描述符
    int fd() const { return fd_; }

    // 获取关注的事件
    uint32_t events() const { return events_; }

    // 设置就绪事件
    void setRevents(uint32_t revents) { revents_ = revents; }

    // 检查是否在epoll中
    bool isInEpoll() const { return inEpoll_; }
    void setInEpoll(bool inEpoll) { inEpoll_ = inEpoll; }

    // 获取所属EventLoop
    EventLoop* loop() const { return loop_; }

    // 检查是否关注写事件
    bool isWriting() const { return events_ & kWriteEvent; }

    // 从epoll中移除
    void remove();

private:
    void update();

    EventLoop* loop_;
    int fd_;
    uint32_t events_;
    uint32_t revents_;
    bool inEpoll_;

    std::function<void()> readCallback_;
    std::function<void()> writeCallback_;
    std::function<void()> closeCallback_;
    std::function<void()> errorCallback_;
};