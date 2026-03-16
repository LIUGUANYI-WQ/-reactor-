#include "TcpConnection.h"
#include "EventLoop.h"
#include "Channel.h"
#include <unistd.h>
#include <sys/socket.h>
#include <iostream>
#include <cerrno>
#include <cstring>

TcpConnection::TcpConnection(EventLoop* loop, int sockfd)
    : loop_(loop),
      sockfd_(sockfd),
      channel_(std::make_unique<Channel>(loop, sockfd)) {

    channel_->setReadCallback([this] { handleRead(); });
    channel_->setWriteCallback([this] { handleWrite(); });
    channel_->setCloseCallback([this] { handleClose(); });
    channel_->setErrorCallback([this] { handleError(); });
}

TcpConnection::~TcpConnection() {
    if (sockfd_ >= 0) {
        ::close(sockfd_);
    }
}

void TcpConnection::connectEstablished() {
    channel_->enableReading();
}

void TcpConnection::connectDestroyed() {
    channel_->disableAll();
    channel_->remove();
}

void TcpConnection::send(const std::string& data) {
    if (loop_->isInLoopThread()) {
        sendInLoop(data);
    } else {
        loop_->queueInLoop([this, data] { sendInLoop(data); });
    }
}

void TcpConnection::shutdown() {
    if (loop_->isInLoopThread()) {
        ::shutdown(sockfd_, SHUT_WR);
    } else {
        loop_->queueInLoop([this] { ::shutdown(sockfd_, SHUT_WR); });
    }
}

void TcpConnection::handleRead() {
    char buf[65536];
    ssize_t n = ::read(sockfd_, buf, sizeof(buf));

    if (n > 0) {
        inputBuffer_.append(buf, n);
        if (messageCallback_) {
            messageCallback_(shared_from_this(), &inputBuffer_);
        }
    } else if (n == 0) {
        handleClose();
    } else {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            handleError();
        }
    }
}

void TcpConnection::handleWrite() {
    if (outputBuffer_.readableBytes() == 0) {
        channel_->disableWriting();
        if (writeCompleteCallback_) {
            writeCompleteCallback_(shared_from_this());
        }
        return;
    }

    ssize_t n = ::write(sockfd_, outputBuffer_.peek(), outputBuffer_.readableBytes());

    if (n > 0) {
        outputBuffer_.retrieve(n);
        if (outputBuffer_.readableBytes() == 0) {
            channel_->disableWriting();
            if (writeCompleteCallback_) {
                writeCompleteCallback_(shared_from_this());
            }
        }
    } else {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            handleError();
        }
    }
}

void TcpConnection::handleClose() {
    channel_->disableAll();
    if (closeCallback_) {
        closeCallback_(shared_from_this());
    }
}

void TcpConnection::handleError() {
    int err;
    socklen_t len = sizeof(err);
    if (::getsockopt(sockfd_, SOL_SOCKET, SO_ERROR, &err, &len) < 0) {
        err = errno;
    }
    std::cerr << "TcpConnection error: " << strerror(err) << std::endl;
}

void TcpConnection::sendInLoop(const std::string& data) {
    ssize_t nwrote = 0;
    size_t remaining = data.size();

    // 如果输出缓冲区为空，直接尝试发送
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
        nwrote = ::write(sockfd_, data.data(), data.size());

        if (nwrote >= 0) {
            remaining = data.size() - nwrote;
            if (remaining == 0 && writeCompleteCallback_) {
                writeCompleteCallback_(shared_from_this());
            }
        } else {
            nwrote = 0;
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                handleError();
                return;
            }
        }
    }

    // 还有数据未发送，放入缓冲区
    if (remaining > 0) {
        outputBuffer_.append(data.data() + nwrote, remaining);
        if (!channel_->isWriting()) {
            channel_->enableWriting();
        }
    }
}