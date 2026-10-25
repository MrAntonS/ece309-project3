// src/sentinel_scanner.cpp
// YOU IMPLEMENT — bounded-memory streaming scan (spec §3.4).
#include "core/sentinel_scanner.h"

SentinelScanner::SentinelScanner(std::string sentinel)
    : sentinel_(std::move(sentinel)) {}

SentinelScanner::Out SentinelScanner::feed(std::string_view chunk) {
    if (found_) return {"", true};
    if (sentinel_.empty()) return {std::string(chunk), false};

    std::string buf;
    buf.reserve(pending_.size() + chunk.size());
    buf += pending_;
    buf += chunk;

    std::size_t pos = buf.find(sentinel_);
    if (pos != std::string::npos) {
        found_ = true;
        pending_.clear();
        return {buf.substr(0, pos), true};
    }

    std::size_t keep = sentinel_.size() - 1;
    if (buf.size() <= keep) {
        pending_ = std::move(buf);
        return {"", false};
    }
    std::string safe = buf.substr(0, buf.size() - keep);
    pending_ = buf.substr(buf.size() - keep);
    return {safe, false};
}

SentinelScanner::Out SentinelScanner::flush() {
    if (found_) return {"", true};
    std::string safe = std::move(pending_);
    pending_.clear();
    return {safe, false};
}
