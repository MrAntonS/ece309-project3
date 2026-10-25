// src/conversation.cpp
// YOU IMPLEMENT — Rule of Five + doubling growth (amortized O(1) append).
#include "core/conversation.h"
#include <stdexcept>
#include <utility>

Conversation::Conversation() = default;

Conversation::~Conversation() { delete[] data_; }

Conversation::Conversation(const Conversation& other)
    : data_(nullptr), size_(0), capacity_(0) {
    if (other.size_ > 0) {
        data_ = new Message[other.capacity_];
        try {
            for (std::size_t i = 0; i < other.size_; ++i) data_[i] = other.data_[i];
        } catch (...) {
            delete[] data_;
            data_ = nullptr;
            throw;
        }
        size_ = other.size_;
        capacity_ = other.capacity_;
    }
}

Conversation& Conversation::operator=(const Conversation& other) {
    if (this == &other) return *this;
    Message* fresh = nullptr;
    std::size_t fresh_cap = 0;
    if (other.size_ > 0) {
        fresh = new Message[other.capacity_];
        try {
            for (std::size_t i = 0; i < other.size_; ++i) fresh[i] = other.data_[i];
        } catch (...) {
            delete[] fresh;
            throw;
        }
        fresh_cap = other.capacity_;
    }
    delete[] data_;
    data_ = fresh;
    size_ = other.size_;
    capacity_ = fresh_cap;
    return *this;
}

Conversation::Conversation(Conversation&& other) noexcept
    : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
}

Conversation& Conversation::operator=(Conversation&& other) noexcept {
    if (this == &other) return *this;
    delete[] data_;
    data_ = other.data_;
    size_ = other.size_;
    capacity_ = other.capacity_;
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
    return *this;
}

void Conversation::reserve(std::size_t new_cap) {
    if (new_cap <= capacity_) return;
    Message* fresh = new Message[new_cap];
    try {
        for (std::size_t i = 0; i < size_; ++i) fresh[i] = std::move(data_[i]);
    } catch (...) {
        delete[] fresh;
        throw;
    }
    delete[] data_;
    data_ = fresh;
    capacity_ = new_cap;
}

void Conversation::append(Message m) {
    if (size_ == capacity_) reserve(capacity_ == 0 ? 1 : capacity_ * 2);
    data_[size_] = std::move(m);
    ++size_;
}

std::size_t Conversation::size() const noexcept { return size_; }
std::size_t Conversation::capacity() const noexcept { return capacity_; }

const Message& Conversation::at(std::size_t i) const {
    if (i >= size_) throw std::out_of_range("Conversation::at: index out of range");
    return data_[i];
}

const Message* Conversation::begin() const noexcept { return data_; }
const Message* Conversation::end() const noexcept { return data_ + size_; }
