#include "Buffer.h"
#include <algorithm>

Buffer::Buffer()
    : buffer_(kInitialSize),
      readerIndex_(0),
      writerIndex_(0) {
}

Buffer::~Buffer() = default;

void Buffer::append(const char* data, size_t len) {
    ensureWritableBytes(len);
    std::copy(data, data + len, beginWrite());
    hasWritten(len);
}

void Buffer::append(const std::string& data) {
    append(data.data(), data.size());
}

size_t Buffer::readableBytes() const {
    return writerIndex_ - readerIndex_;
}

size_t Buffer::writableBytes() const {
    return buffer_.size() - writerIndex_;
}

const char* Buffer::peek() const {
    return begin() + readerIndex_;
}

void Buffer::retrieve(size_t len) {
    if (len < readableBytes()) {
        readerIndex_ += len;
    } else {
        retrieveAll();
    }
}

void Buffer::retrieveAll() {
    readerIndex_ = 0;
    writerIndex_ = 0;
}

std::string Buffer::retrieveAllAsString() {
    std::string result(peek(), readableBytes());
    retrieveAll();
    return result;
}

void Buffer::ensureWritableBytes(size_t len) {
    if (writableBytes() < len) {
        if (writableBytes() + readerIndex_ < len) {
            buffer_.resize(writerIndex_ + len);
        } else {
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_, begin() + writerIndex_, begin());
            readerIndex_ = 0;
            writerIndex_ = readerIndex_ + readable;
        }
    }
}

char* Buffer::beginWrite() {
    return begin() + writerIndex_;
}

void Buffer::hasWritten(size_t len) {
    writerIndex_ += len;
}

const char* Buffer::findCRLF() const {
    const char* crlf = std::search(peek(), begin() + writerIndex_, "\r\n", "\r\n" + 2);
    return crlf == begin() + writerIndex_ ? nullptr : crlf;
}

char* Buffer::begin() {
    return &*buffer_.begin();
}

const char* Buffer::begin() const {
    return &*buffer_.begin();
}