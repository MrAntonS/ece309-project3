// include/core/conversation.h
// YOU IMPLEMENT — growable array, Rule of Five (spec §3.2).
// No std::vector allowed here. Raw new/delete only in this class.
#pragma once
#include "core/message.h"
#include <cstddef>

class Conversation {
public:
    Conversation();
    ~Conversation();

    Conversation(const Conversation& other);
    Conversation& operator=(const Conversation& other);

    Conversation(Conversation&& other) noexcept;
    Conversation& operator=(Conversation&& other) noexcept;

    void append(Message m);

    std::size_t size() const noexcept;
    std::size_t capacity() const noexcept;  // exposed for growth tests

    const Message& at(std::size_t i) const;

    const Message* begin() const noexcept;
    const Message* end() const noexcept;

private:
    Message* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;

    void reserve(std::size_t new_cap);
};
