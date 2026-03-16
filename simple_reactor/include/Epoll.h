#pragma once

/**
 * @file Epoll.h
 * @brief 多路复用封装 - 封装 epoll_create/epoll_ctl/epoll_wait
 */

#include <sys/epoll.h>
#include <vector>
#include <memory>

class Channel;

class Epoll {
public:
    Epoll();
    ~Epoll();

    // 添加或修改Channel
    void updateChannel(Channel* channel);

    // 从epoll中删除Channel
    void removeChannel(Channel* channel);

    // 等待事件，返回就绪的Channel列表
    std::vector<Channel*> poll(int timeoutMs);

private:
    static const int kMaxEvents = 1024;

    int epollFd_;
    std::vector<epoll_event> events_;
};
