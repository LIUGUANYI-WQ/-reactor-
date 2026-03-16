#pragma once

/**
 * @file Buffer.h
 * @brief 缓冲区类 - 解决粘包、非阻塞读写数据缓存
 */

#include <string>
#include <cstring>
#include <vector>

class Buffer {
public:
    Buffer();
    ~Buffer();

    // 追加数据
    void append(const char* data, size_t len);
    void append(const std::string& data);

    // 获取可读数据长度
    size_t readableBytes() const;

    // 获取可写空间
    size_t writableBytes() const;

    // 查看数据（不取出）
    const char* peek() const;

    // 取出数据
    void retrieve(size_t len);
    void retrieveAll();

    // 读取所有数据为string
    std::string retrieveAllAsString();

    // 确保可写空间
    void ensureWritableBytes(size_t len);

    // 获取底层数据指针
    char* beginWrite();

    // 移动写指针
    void hasWritten(size_t len);

    // 查找CRLF（用于HTTP协议）
    const char* findCRLF() const;

private:
    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;

    static const size_t kInitialSize = 1024;

    char* begin();
    const char* begin() const;
};
