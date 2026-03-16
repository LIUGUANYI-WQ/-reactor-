#include "Acceptor.h"
#include "EventLoop.h"
#include "Channel.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <cerrno>

Acceptor::Acceptor(EventLoop* loop, int port)
    : loop_(loop),
      port_(port),
      listenFd_(-1),
      idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC)) {

    listenFd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (listenFd_ < 0) {
        std::cerr << "socket create failed: " << strerror(errno) << std::endl;
        return;
    }

    int reuse = 1;
    if (::setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        std::cerr << "setsockopt SO_REUSEADDR failed: " << strerror(errno) << std::endl;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (::bind(listenFd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "bind failed: " << strerror(errno) << std::endl;
        ::close(listenFd_);
        listenFd_ = -1;
        return;
    }

    acceptChannel_ = std::make_unique<Channel>(loop, listenFd_);
    acceptChannel_->setReadCallback([this] { handleRead(); });
}

Acceptor::~Acceptor() {
    acceptChannel_->disableAll();
    acceptChannel_->remove();
    if (listenFd_ >= 0) {
        ::close(listenFd_);
    }
    ::close(idleFd_);
}

void Acceptor::listen() {
    if (::listen(listenFd_, SOMAXCONN) < 0) {
        std::cerr << "listen failed: " << strerror(errno) << std::endl;
        return;
    }
    acceptChannel_->enableReading();
}

void Acceptor::handleRead() {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    int connfd = ::accept4(listenFd_, (struct sockaddr*)&addr, &len,
                           SOCK_NONBLOCK | SOCK_CLOEXEC);

    if (connfd >= 0) {
        if (newConnectionCallback_) {
            newConnectionCallback_(connfd);
        } else {
            ::close(connfd);
        }
    } else {
        // 处理文件描述符耗尽
        if (errno == EMFILE || errno == ENFILE) {
            ::close(idleFd_);
            idleFd_ = ::accept(listenFd_, (struct sockaddr*)&addr, &len);
            ::close(idleFd_);
            idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        }
    }
}
